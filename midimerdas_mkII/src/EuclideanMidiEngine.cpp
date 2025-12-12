#include "MIDIRouter.h"
#include "EuclideanMidiEngine.h"
#include "OSCController.h"
#include <Control_Surface.h>
#include <Adafruit_TinyUSB.h>
// FreeRTOS for worker task
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Tabela de ticks por resolução
const uint8_t EuclideanMidiEngine::ticksPerResolution[4] = {24, 12, 6, 3};

// Instance global para acessar métodos via function pointers
static EuclideanMidiEngine* g_euclidMidiEngine = nullptr;

// Wrappers estáticos para callbacks (apenas stepStart é necessário)
void euclidStepStartWrapper(uint8_t step) {
	if (g_euclidMidiEngine) g_euclidMidiEngine->onStepStart(step);
}

void EuclideanMidiEngine::begin(EuclideanSequencer* seq, MidiClock* clk,
                                 BluetoothMIDI_Interface* ble,
                                 Adafruit_USBD_MIDI* usb) {
	euclSeq = seq;
	clock = clk;
	ble_midi = ble;
	usb_midi = usb;
	
	// Inicializa cache e fila de Note Offs
	for (uint8_t i = 0; i < 8; ++i) {
		pendingNoteOffs[i].pending = false;
		pendingNoteOffs[i].note = 0;
		pendingNoteOffs[i].channel = 0;
		pendingNoteOffs[i].time = 0;
	}
	
	// Guarda instância global para callbacks
	g_euclidMidiEngine = this;
	
	// Registra callbacks do sequenciador
	if (euclSeq) {
		euclSeq->onSendMidi = [](uint8_t status, uint8_t data1, uint8_t data2) {
			if (g_euclidMidiEngine) {
				g_euclidMidiEngine->onSequencerNote(status, data1, data2);
			}
		};
	}
	
	// Registra callbacks do clock
	registerClockCallbacks();

	// Cria semáforo e tarefa worker para processar a fila de MIDI
	if (!midiQueueSem) {
		midiQueueSem = xSemaphoreCreateCounting(MIDI_QUEUE_SIZE, 0);
	}
	if (!midiWorkerHandle) {
		xTaskCreatePinnedToCore(
			EuclideanMidiEngine::midiWorkerTask,
			"MIDIWorker",
			4096,
			this,
			2,
			&midiWorkerHandle,
			1);
	}
}

// ===== FILA UNIFICADA PARA USB MIDI =====

void EuclideanMidiEngine::enqueueMidiEvent(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t noteLength, bool isNoteOn) {
	uint16_t nextHead = (midiQueueHead + 1) % MIDI_QUEUE_SIZE;
	
	if (nextHead == midiQueueTail) {
		midiDroppedEvents++;
		return;
	}
	
	midiQueue[midiQueueHead].channel = channel & 0x0F;
	midiQueue[midiQueueHead].note = note & 0x7F;
	// Velocity já é 0-127, apenas garantir range
	midiQueue[midiQueueHead].velocity = constrain(velocity, 0, 127);
	midiQueue[midiQueueHead].noteLength = constrain(noteLength, 50, 700);  // Validar range 50-700
	midiQueue[midiQueueHead].isNoteOn = isNoteOn;
	midiQueueHead = nextHead;

	// Sinaliza ao worker que há um evento novo
	if (midiQueueSem) {
		xSemaphoreGive(midiQueueSem);
	}
}

void EuclideanMidiEngine::processMidiQueue() {
	while (midiQueueHead != midiQueueTail) {
		MidiEvent& evt = midiQueue[midiQueueTail];
		uint8_t statusByte = (evt.isNoteOn ? 0x90 : 0x80) | (evt.channel & 0x0F);
		
		// Enviar baseado no protocolo selecionado
		// Enviar baseado no protocolo selecionado (respeita a seleção Note Out)
		uint8_t outs = 0x0F; // default all
		if (euclSeq) outs = (uint8_t)euclSeq->getOutputNotes();
		// USB
		if ((outs & EuclideanSequencer::OUT_USB) && usb_midi) {
			usb_midi->write(statusByte);
			usb_midi->write(evt.note);
			usb_midi->write(evt.velocity);
		}
		// BLE
		if ((outs & EuclideanSequencer::OUT_BLE) && ble_midi) {
			using namespace cs;
			ble_midi->send(ChannelMessage(statusByte, evt.note, evt.velocity));
			ble_midi->sendNow();
		}
		// DINs
		if (outs & EuclideanSequencer::OUT_DIN) {
			MIDIRouter::sendToOutput(0, statusByte);  // DIN1
			MIDIRouter::sendToOutput(0, evt.note);
			MIDIRouter::sendToOutput(0, evt.velocity);
			MIDIRouter::sendToOutput(1, statusByte);  // DIN2
			MIDIRouter::sendToOutput(1, evt.note);
			MIDIRouter::sendToOutput(1, evt.velocity);
			MIDIRouter::sendToOutput(2, statusByte);  // DIN3
			MIDIRouter::sendToOutput(2, evt.note);
			MIDIRouter::sendToOutput(2, evt.velocity);
		}
		
		midiQueueTail = (midiQueueTail + 1) % MIDI_QUEUE_SIZE;
	}
}

// Worker task que consome a fila de eventos MIDI
void EuclideanMidiEngine::midiWorkerTask(void* pvParameters) {
	EuclideanMidiEngine* engine = reinterpret_cast<EuclideanMidiEngine*>(pvParameters);
	if (!engine) {
		vTaskDelete(nullptr);
		return;
	}
	for (;;) {
		// Aguarda até haver eventos enfileirados
		if (engine->midiQueueSem && xSemaphoreTake(engine->midiQueueSem, pdMS_TO_TICKS(200)) == pdTRUE) {
			// Processa enquanto houver eventos
			engine->processMidiQueue();
		} else {
			// Sem evento, pequena espera para liberar CPU
			vTaskDelay(pdMS_TO_TICKS(1));
		}
	}
}

void EuclideanMidiEngine::onSequencerNote(uint8_t status, uint8_t data1, uint8_t data2) {
	if (!euclSeq) return;
	
	uint8_t channel = status & 0x0F;
	uint8_t msgType = status & 0xF0;
	
	// Procura a primeira track com este canal MIDI para obter noteLength
	uint16_t noteLength = 100;  // padrão
	for (uint8_t trackIdx = 0; trackIdx < 8; trackIdx++) {
		if (euclSeq->getTrackMidiChannel(trackIdx) == channel) {
			noteLength = euclSeq->getTrackNoteLength(trackIdx);
			break;
		}
	}
	
	if (msgType == 0x90) {  // Note On
		enqueueMidiEvent(channel, data1, data2, noteLength, true);
	} else if (msgType == 0x80) {  // Note Off
		enqueueMidiEvent(channel, data1, data2, noteLength, false);
	}
}

void EuclideanMidiEngine::registerClockCallbacks() {
	if (!clock) return;
	clock->setStepStartCallback(euclidStepStartWrapper);
}

void EuclideanMidiEngine::onStepStart(uint8_t step) {
	if (!euclSeq || !clock) return;
	
	// Usa contador global de TICKS PPQN do clock para calcular steps euclidiano
	uint32_t globalTickCount = clock->getTickCount();
	
	// Atualiza passo visual da track selecionada
	uint8_t selectedPattern = euclSeq->getSelectedPattern();
	uint8_t selectedResolution = euclSeq->getTrackResolution(selectedPattern);
	uint8_t selectedSteps = euclSeq->getSteps();
	
	// Calcula o step euclidiano para a track selecionada
	// ticksPerResolution[res-1] = número de TICKS PPQN para um step euclidiano
	uint16_t ticksPerEuclStep = ticksPerResolution[selectedResolution - 1];
	uint8_t selectedEuclStep = (globalTickCount / ticksPerEuclStep) % selectedSteps;
	euclSeq->setCurrentStep(selectedEuclStep);
	
	// Polyphony: verifica todas as tracks ativas e habilitadas
	for (uint8_t trackIdx = 0; trackIdx < 8; ++trackIdx) {
		if (euclSeq->isTrackActive(trackIdx) && euclSeq->isTrackEnabled(trackIdx)) {
			uint8_t trackResolution = euclSeq->getTrackResolution(trackIdx);
			uint16_t trackTicksPerStep = ticksPerResolution[trackResolution - 1];
			uint8_t trackSteps = euclSeq->getTrackSteps(trackIdx);
			
			// Calcula o step euclidiano ATUAL para esta track
			uint8_t trackEuclStep = (globalTickCount / trackTicksPerStep) % trackSteps;
			euclSeq->setTrackCurrentStep(trackIdx, trackEuclStep);
			
			// Compara com o ÚLTIMO STEP PROCESSADO (não com o anterior)
			// Isso evita duplicatas quando há múltiplos ticks
			if (trackEuclStep != lastProcessedStep[trackIdx]) {
				// Marca este step como processado
				lastProcessedStep[trackIdx] = trackEuclStep;
				
				// Verifica padrão euclidiano
			if (euclSeq->getTrackPatternBit(trackIdx, trackEuclStep)) {
				uint8_t note = euclSeq->getTrackNote(trackIdx);
				uint8_t velocity = euclSeq->getTrackVelocity(trackIdx);
				uint8_t channel = euclSeq->getTrackMidiChannel(trackIdx);
				uint8_t status = 0x90 | channel;
				euclSeq->onSendMidi(status, note, velocity);
				
			// Agenda note-off para esta track (usando duração fixa de 100ms)
			pendingNoteOffs[trackIdx].pending = true;
			pendingNoteOffs[trackIdx].note = note;
			pendingNoteOffs[trackIdx].channel = channel & 0x0F;  // Garante apenas 4 bits para canal
			pendingNoteOffs[trackIdx].velocity = velocity;  // Armazena velocity original
			uint16_t noteLength = euclSeq->getTrackNoteLength(trackIdx);  // Obtém duração configurada
			pendingNoteOffs[trackIdx].noteLength = noteLength;  // Armazena para usar no NOTE OFF
			pendingNoteOffs[trackIdx].time = (uint32_t)millis() + noteLength;
			}
			}
		}
	}
}

void EuclideanMidiEngine::update() {
	// Processa todos os Note Offs pendentes
	uint32_t now = millis();
	for (uint8_t t = 0; t < 8; ++t) {
		if (pendingNoteOffs[t].pending && now >= pendingNoteOffs[t].time) {
			pendingNoteOffs[t].pending = false;
			// Enfileira NOTE OFF com noteLength armazenado
			enqueueMidiEvent(pendingNoteOffs[t].channel, pendingNoteOffs[t].note, 
							 pendingNoteOffs[t].velocity, pendingNoteOffs[t].noteLength, false);
		}
	}
	
		// Processa fila unificada de MIDI (USB)
		// Nota: a fila MIDI agora é processada pela task `MIDIWorker` para evitar bloqueios.
}

bool EuclideanMidiEngine::isOSCClientConnected() const {
    if (!osc) return false;
    // Cliente é considerado conectado enquanto há clientes P2P registados
    return osc->getActiveP2PClientCount() > 0;
}
