#include "MidiFeedback.h"
#include "EuclideanSequencer.h"
#include "EuclideanHarmonicSequencer.h"
#include "MidiClock.h"
#include "MidiCCMapping.h"
#include "MIDIRouter.h"
#include <Adafruit_TinyUSB.h>
// FreeRTOS queue/task
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// Local base note for DUB per-track mapping (matches MidiCCMapping note base)
static const uint8_t DUB_NOTE_BASE = 76;

// Instâncias estáticas
Adafruit_USBD_MIDI* MidiFeedback::usb_midi = nullptr;
bool MidiFeedback::feedbackEnabled = true;
EuclideanSequencer* MidiFeedback::eucl_seq = nullptr;

// Async dispatcher internals
static QueueHandle_t feedbackQueue = nullptr;
static const int FEEDBACK_QUEUE_SIZE = 128; // capacidade fixa

struct FeedbackMessage {
	uint8_t status;
	uint8_t data1;
	uint8_t data2;
};

void MidiFeedback::midiDispatchTask(void* param) {
	FeedbackMessage msg;
	while (true) {
		if (feedbackQueue && xQueueReceive(feedbackQueue, &msg, pdMS_TO_TICKS(50)) == pdTRUE) {
			// USB
			if (usb_midi) {
				usb_midi->write(msg.status);
				usb_midi->write(msg.data1);
				usb_midi->write(msg.data2);
			}
			// BLE removed
			// DIN outputs (send to all configured DINs)
			MIDIRouter::sendToOutput(0, msg.status);
			MIDIRouter::sendToOutput(0, msg.data1);
			MIDIRouter::sendToOutput(0, msg.data2);
			MIDIRouter::sendToOutput(1, msg.status);
			MIDIRouter::sendToOutput(1, msg.data1);
			MIDIRouter::sendToOutput(1, msg.data2);
			MIDIRouter::sendToOutput(2, msg.status);
			MIDIRouter::sendToOutput(2, msg.data1);
			MIDIRouter::sendToOutput(2, msg.data2);
		} else {
			vTaskDelay(pdMS_TO_TICKS(1));
		}
	}
}

// Valores anteriores para detecção de mudanças
uint8_t MidiFeedback::lastStepsCC = 0xFF;
uint8_t MidiFeedback::lastHitsCC = 0xFF;
uint8_t MidiFeedback::lastOffsetCC = 0xFF;
uint8_t MidiFeedback::lastNoteCC = 0xFF;
uint8_t MidiFeedback::lastVelocityCC = 0xFF;
uint8_t MidiFeedback::lastChannelCC = 0xFF;
uint8_t MidiFeedback::lastResolutionCC = 0xFF;
uint8_t MidiFeedback::lastTrackCC = 0xFF;
uint8_t MidiFeedback::lastPlayCC = 0xFF;
uint8_t MidiFeedback::lastTempoCC = 0xFF;
uint8_t MidiFeedback::lastNoteLengthCC = 0xFF;
int8_t MidiFeedback::lastDubState[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

// Harmonic last-value CC caches
int32_t MidiFeedback::lastHarmonicTonalityCC = -1;
int32_t MidiFeedback::lastHarmonicScaleCC = -1;
int32_t MidiFeedback::lastHarmonicModeCC = -1;
int32_t MidiFeedback::lastHarmonicStepsCC = -1;
int32_t MidiFeedback::lastHarmonicHitsCC = -1;
int32_t MidiFeedback::lastHarmonicOffsetCC = -1;
int32_t MidiFeedback::lastHarmonicPolyCC = -1;
int32_t MidiFeedback::lastHarmonicVelocityCC = -1;
int32_t MidiFeedback::lastHarmonicNoteLengthCC = -1;
int32_t MidiFeedback::lastHarmonicChordSlotCC[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
int32_t MidiFeedback::lastHarmonicOctaveCC = -100;

void MidiFeedback::begin(Adafruit_USBD_MIDI* usb) {
	usb_midi = usb;
	feedbackEnabled = true;
	// ensure dispatcher exists
	initDispatcher();
}

void MidiFeedback::setEuclideanSequencer(EuclideanSequencer* seq) {
    eucl_seq = seq;
}

// Envia todos os CCs independentemente do cache de valores anteriores
void MidiFeedback::sendAllFeedbackForced(EuclideanSequencer* seq, MidiClock* clock) {
	if (!feedbackEnabled || !seq) return;
	uint8_t channel = 9;
	// Mapping preference removed: always send MIDI feedback
	bool midiEnabled = true;
	auto sendDirect = [&](uint8_t cc, uint8_t value) {
		if (!midiEnabled) return;
		uint8_t statusByte = 0xB0 | (channel & 0x0F);
		enqueueRaw(statusByte, cc & 0x7F, value & 0x7F);
	};

	// Steps
	uint8_t steps = seq->getSteps();
	uint8_t stepsCC = constrain(map(steps, 1, 32, 0, 127), 0, 127);
	sendDirect(20, stepsCC); lastStepsCC = stepsCC;

	// Hits
	uint8_t hits = seq->getHits();
	uint8_t hitsCC = constrain(map(hits, 1, 32, 0, 127), 0, 127);
	sendDirect(21, hitsCC); lastHitsCC = hitsCC;

	// Offset (user-facing)
	uint8_t offset = seq->getOffset();
	uint8_t offsetUser = offset + 1;
	uint8_t offsetCC = map(offsetUser, 1, 32, 0, 127);
	sendDirect(22, offsetCC); lastOffsetCC = offsetCC;

	// Note
	uint8_t note = seq->getNote(); sendDirect(23, note); lastNoteCC = note;

	// Velocity
	uint8_t velocity = seq->getVelocity(); sendDirect(19, velocity); lastVelocityCC = velocity;

	// Resolution
	uint8_t res = seq->getResolution(); uint8_t resCC = map(res, 1, 4, 0, 127);
	sendDirect(25, resCC); lastResolutionCC = resCC;

	// Track
	uint8_t track = seq->getSelectedPattern(); uint8_t trackCC = map(track, 0, 7, 0, 127);
	sendDirect(26, trackCC); lastTrackCC = trackCC;

	// Channel
	uint8_t channelVal = seq->getMidiChannel(); uint8_t channelCC = constrain(map(channelVal, 0, 15, 0, 127), 0, 127);
	sendDirect(24, channelCC); lastChannelCC = channelCC;

	// Note length
	uint16_t noteLength = seq->getNoteLength(); uint8_t noteLengthCC = constrain(map(noteLength, 50, 700, 0, 127), 0, 127);
	sendDirect(31, noteLengthCC); lastNoteLengthCC = noteLengthCC;

	// Tempo
	if (clock) {
		uint8_t tempoCC;
		if (clock->isSlave()) tempoCC = 127; else tempoCC = constrain(map((int)clock->getBPM(), 30, 240, 0, 126), 0, 126);
		sendDirect(28, tempoCC); lastTempoCC = tempoCC;
	}

	// Dub notes: send 8 notes (one per track) as forced snapshot
	for (uint8_t i = 0; i < 8; ++i) {
		bool dubEnabled = seq->isTrackEnabled(i);
		lastDubState[i] = dubEnabled ? 1 : 0;
		if (midiEnabled) {
			uint8_t note = DUB_NOTE_BASE + i;
			MidiFeedback::sendNote(note, dubEnabled ? 127 : 0);
		}
	}
	// CC 57: routing flags (bit0=USB,bit1=DIN,bit2=BLE) - derive from global euclidean sequencer if set
	{
		// CC 57 (routing flags) removed: avoid sending routing feedback from sequencer
		// Historically this reported output routing (USB/DIN/BLE). Emitting this
		// control caused undesired external mapping; keep routing state internal.
	}
}

// Harmonic sequencer MIDI feedback (forced snapshot)
void MidiFeedback::sendAllHarmonicFeedbackForced(EuclideanHarmonicSequencer* hseq, MidiClock* clock) {
	if (!feedbackEnabled || !hseq) return;
	uint8_t channel = 9;
	// Always send harmonic MIDI feedback
	bool midiEnabled = true;
	auto sendDirect = [&](uint8_t cc, uint8_t value) {
		if (!midiEnabled) return;
		uint8_t statusByte = 0xB0 | (channel & 0x0F);
		enqueueRaw(statusByte, cc & 0x7F, value & 0x7F);
	};

	// CC 40: Tonality (0..11)
	uint8_t ton = hseq->getTonic(); sendDirect(40, ton);
	// CC 41: Scale type index
	uint8_t scale = (uint8_t)hseq->getScaleType(); sendDirect(41, scale);
	// CC 42: Mode (0 = chords, 1 = notes)
	uint8_t mode = (uint8_t)hseq->getDistributionMode(); sendDirect(42, mode);
	// CC 43: Steps (1..32 -> 0..127)
	uint8_t steps = hseq->getSteps(); uint8_t stepsCC = constrain(map(steps, 1, 32, 0, 127), 0, 127); sendDirect(43, stepsCC);
	// CC 44: Hits
	uint8_t hits = hseq->getHits(); uint8_t hitsCC = constrain(map(hits, 1, 32, 0, 127), 0, 127); sendDirect(44, hitsCC);
	// CC 45: Offset (0..31)
	uint8_t off = hseq->getOffset(); uint8_t offCC = constrain(map(off, 0, 31, 0, 127), 0, 127); sendDirect(45, offCC);
	// CC 46: Polyphony (1..MAX_POLYPHONY)
	uint8_t poly = hseq->getPolyphony(); uint8_t polyCC = constrain(map(poly, 1, (int)EuclideanHarmonicSequencer::MAX_POLYPHONY, 0, 127), 0, 127); sendDirect(46, polyCC);
	// CC 47: Velocity
	uint8_t vel = hseq->getVelocity(); sendDirect(47, vel);
	// CC 48: Note length (ms 10..2000)
	uint16_t nlen = hseq->getNoteLength(); uint8_t nlenCC = constrain(map((int)nlen, 10, 2000, 0, 127), 0, 127); sendDirect(48, nlenCC);
	// CC 49: Base octave (-2..+2) mapped to 0..127
	int oct = hseq->getBaseOctave(); uint8_t octCC = constrain(map(oct, -2, 2, 0, 127), 0, 127); sendDirect(49, octCC);

	// Chord slot toggles: NOTES 92..99 reflect slots 0..7
	for (uint8_t i = 0; i < 8; ++i) {
		bool has = (i < hseq->getChordListSize());
		if (midiEnabled) {
			uint8_t note = 92 + i;
			MidiFeedback::sendNote(note, has ? 127 : 0);
		}
	}
}

void MidiFeedback::sendAllHarmonicFeedback(EuclideanHarmonicSequencer* hseq, MidiClock* clock) {
	if (!feedbackEnabled || !hseq) return;
	// Always send harmonic MIDI feedback
	bool midiEnabled = true;

	auto sendIfChangedCC = [&](uint8_t cc, int32_t newValue, int32_t &lastRef) {
		if (lastRef == newValue) return;
		lastRef = newValue;
		uint8_t channel = 9;
		uint8_t status = 0xB0 | (channel & 0x0F);
		uint8_t v = (uint8_t)constrain((int)newValue, 0, 127);
		enqueueRaw(status, cc & 0x7F, v & 0x7F);
	};

	// Tonality CC 40
	sendIfChangedCC(40, (int32_t)hseq->getTonic(), lastHarmonicTonalityCC);
	// Scale CC 41
	sendIfChangedCC(41, (int32_t)hseq->getScaleType(), lastHarmonicScaleCC);
	// Mode CC 42
	sendIfChangedCC(42, (int32_t)hseq->getDistributionMode(), lastHarmonicModeCC);
	// Steps CC 43 (1..32)
	sendIfChangedCC(43, (int32_t)constrain((int)hseq->getSteps(),1,32), lastHarmonicStepsCC);
	// Hits CC 44
	sendIfChangedCC(44, (int32_t)constrain((int)hseq->getHits(),1,32), lastHarmonicHitsCC);
	// Offset CC 45
	sendIfChangedCC(45, (int32_t)hseq->getOffset(), lastHarmonicOffsetCC);
	// Poly CC 46
	sendIfChangedCC(46, (int32_t)hseq->getPolyphony(), lastHarmonicPolyCC);
	// Velocity CC 47
	sendIfChangedCC(47, (int32_t)hseq->getVelocity(), lastHarmonicVelocityCC);
	// Note length CC 48 (ms)
	sendIfChangedCC(48, (int32_t)hseq->getNoteLength(), lastHarmonicNoteLengthCC);
	// Octave CC 49 (-2..+2 mapped to 0..127)
	{
		int32_t octVal = (int32_t)hseq->getBaseOctave();
		int32_t octMapped = (int32_t)constrain(map((int)octVal, -2, 2, 0, 127), 0, 127);
		sendIfChangedCC(49, octMapped, lastHarmonicOctaveCC);
	}

	// Chord slot notes 92..99 - send per-slot when changed
	for (uint8_t i = 0; i < 8; ++i) {
		int32_t exists = (i < hseq->getChordListSize()) ? 1 : 0;
		if (lastHarmonicChordSlotCC[i] != exists) {
			lastHarmonicChordSlotCC[i] = exists;
			uint8_t note = 92 + i;
			MidiFeedback::sendNote(note, exists ? 127 : 0);
		}
	}
}

void MidiFeedback::setBluetoothInterface(void* ble) {
	(void)ble;
	// BLE removed: no-op
}

void MidiFeedback::initDispatcher() {
	if (feedbackQueue) return; // já inicializado
	feedbackQueue = xQueueCreate(FEEDBACK_QUEUE_SIZE, sizeof(FeedbackMessage));
	if (!feedbackQueue) return;
	xTaskCreatePinnedToCore(
		midiDispatchTask,
		"MidiDispatch",
		4096,
		nullptr,
		2,
		nullptr,
		1
	);
}

void MidiFeedback::enqueueRaw(uint8_t status, uint8_t data1, uint8_t data2) {
	if (!feedbackEnabled) return;
	FeedbackMessage msg{status, data1, data2};
	if (!feedbackQueue) {
		// fallback síncrono se fila não disponível
		if (usb_midi) {
			usb_midi->write(status);
			usb_midi->write(data1);
			usb_midi->write(data2);
		}
		// BLE removed: do not send BLE messages here
		MIDIRouter::sendToOutput(0, status);
		MIDIRouter::sendToOutput(0, data1);
		MIDIRouter::sendToOutput(0, data2);
		MIDIRouter::sendToOutput(1, status);
		MIDIRouter::sendToOutput(1, data1);
		MIDIRouter::sendToOutput(1, data2);
		MIDIRouter::sendToOutput(2, status);
		MIDIRouter::sendToOutput(2, data1);
		MIDIRouter::sendToOutput(2, data2);
		return;
	}
	// tenta enfileirar sem bloquear; se cheia, descarta a mensagem
	xQueueSend(feedbackQueue, &msg, 0);
}

void MidiFeedback::sendCCIfChanged(uint8_t cc, uint8_t newValue, uint8_t& lastValue, uint8_t channel) {
	// Função obsoleta - mantida por compatibilidade, mas não mais utilizada
	// A lógica de envio foi integrada diretamente em sendAllFeedback()
	// com suporte ao protocolo de feedback do sequenciador
}

void MidiFeedback::sendAllFeedback(EuclideanSequencer* seq, MidiClock* clock) {
	if (!feedbackEnabled || !seq) return;
	
	uint8_t channel = 9;  // MIDI Control Channel (índice 9 = canal 10)
	// Respect MidiMap ON/OFF
	// Always send MIDI feedback
	bool midiEnabled = true;
	
	// Função lambda auxiliar para enviar com o protocolo (respeita midiEnabled)
	auto sendCCWithProtocol = [&](uint8_t cc, uint8_t newValue, uint8_t& lastValue) {
		if (!midiEnabled) return;
		if (lastValue == newValue) return;
		lastValue = newValue;
		uint8_t statusByte = 0xB0 | (channel & 0x0F);
		enqueueRaw(statusByte, cc & 0x7F, newValue & 0x7F);
	};
	
	// Steps (CC_STEPS = 20)
	uint8_t steps = seq->getSteps();
	uint8_t stepsCC = constrain(map(steps, 1, 32, 0, 127), 0, 127);
	sendCCWithProtocol(20, stepsCC, lastStepsCC);
	
	// Hits (CC_HITS = 21)
	uint8_t hits = seq->getHits();
	uint8_t hitsCC = constrain(map(hits, 1, 32, 0, 127), 0, 127);
	sendCCWithProtocol(21, hitsCC, lastHitsCC);
	
	// Offset (CC_OFFSET = 22) - enviar user-facing 1..N value
	uint8_t offset = seq->getOffset();
	uint8_t offsetUser = offset + 1; // convert internal 0..N-1 to 1..N
	uint8_t offsetCC = map(offsetUser, 1, 32, 0, 127);
	sendCCWithProtocol(22, offsetCC, lastOffsetCC);
	
	// Note (CC_NOTE = 23)
	uint8_t note = seq->getNote();
	sendCCWithProtocol(23, note, lastNoteCC);
	
	// Velocity (CC_VELOCITY = 19)
	uint8_t velocity = seq->getVelocity();
	sendCCWithProtocol(19, velocity, lastVelocityCC);
	
	// Resolution (CC_RESOLUTION = 25)
	uint8_t res = seq->getResolution();
	uint8_t resCC = map(res, 1, 4, 0, 127);
	sendCCWithProtocol(25, resCC, lastResolutionCC);
	
	// Track (CC_TRACK = 26)
	uint8_t track = seq->getSelectedPattern();
	uint8_t trackCC = map(track, 0, 7, 0, 127);
	sendCCWithProtocol(26, trackCC, lastTrackCC);

	// Channel (CC_CHANNEL = 24)
	uint8_t channelVal = seq->getMidiChannel();
	uint8_t channelCC = constrain(map(channelVal, 0, 15, 0, 127), 0, 127);
	sendCCWithProtocol(24, channelCC, lastChannelCC);

	// Note Length (CC_NOTE_LENGTH = 31)
	uint16_t noteLength = seq->getNoteLength();
	uint8_t noteLengthCC = constrain(map(noteLength, 50, 700, 0, 127), 0, 127);
	sendCCWithProtocol(31, noteLengthCC, MidiFeedback::lastNoteLengthCC);
	
	// Play state feedback: enviar Note 0 (universal toggle) somente em mudança de estado
	if (seq && clock) {
		uint8_t playState = clock->isRunningState() ? 1 : 0;
		if (playState != lastPlayCC) {
			lastPlayCC = playState;
			MidiFeedback::sendNote(0, playState ? 127 : 0);
		}
	}
	// Não enviamos mais CC 27 como feedback para evitar uso de CC para transporte.
	
	// Tempo (CC_TEMPO = 28)
	if (clock) {
		uint8_t tempoCC;
		if (clock->isSlave()) {
			tempoCC = 127;  // Valor especial para SLAVE
		} else {
			float bpm = clock->getBPM();
			tempoCC = constrain(map((int)bpm, 30, 240, 0, 126), 0, 126);
		}
		sendCCWithProtocol(28, tempoCC, lastTempoCC);
	}

	// Dub notes: send per-track note only when state changed
	for (uint8_t i = 0; i < 8; ++i) {
		bool dubEnabled = seq->isTrackEnabled(i);
		int8_t last = lastDubState[i];
		int8_t cur = dubEnabled ? 1 : 0;
		if (last != cur) {
			lastDubState[i] = cur;
			if (midiEnabled) {
				uint8_t note = DUB_NOTE_BASE + i;
				MidiFeedback::sendNote(note, dubEnabled ? 127 : 0);
			}
		}
	}

}

void MidiFeedback::sendNote(uint8_t note, uint8_t velocity, uint8_t channel) {
	if (!feedbackEnabled) return;

	// Always allow sending note feedback regardless of per-sequencer flag
	uint8_t status = (velocity > 0) ? (0x90 | (channel & 0x0F)) : (0x80 | (channel & 0x0F));
	enqueueRaw(status, note & 0x7F, velocity & 0x7F);
}

void MidiFeedback::sendCC(uint8_t cc, uint8_t value, uint8_t channel) {
	if (!feedbackEnabled) return;
	uint8_t status = 0xB0 | (channel & 0x0F);
	enqueueRaw(status, cc & 0x7F, value & 0x7F);
}

void MidiFeedback::resetFeedbackState() {
	lastStepsCC = 0xFF;
	lastHitsCC = 0xFF;
	lastOffsetCC = 0xFF;
	lastNoteCC = 0xFF;
	lastVelocityCC = 0xFF;
	lastChannelCC = 0xFF;
	lastResolutionCC = 0xFF;
	lastTrackCC = 0xFF;
	lastPlayCC = 0xFF;
	lastTempoCC = 0xFF;
	lastNoteLengthCC = 0xFF;
	for (int i = 0; i < 8; ++i) lastDubState[i] = -1;
}

