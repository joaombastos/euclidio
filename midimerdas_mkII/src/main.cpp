#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Control_Surface.h>
#include <Adafruit_TinyUSB.h>
#include <USB.h>

#include "Pinos.h"
#include "RoutingMatrix.h"
#include "UI.h"
#include "Encoder.h"
#include "EuclideanSequencer.h"
#include "MidiClock.h"
#include "MIDIRouter.h"
#include "EuclideanMidiEngine.h"
#include "MidiFeedback.h"
#include "MidiCCMapping.h"
#include "OSCController.h"
#include "OSCMapping.h"
#include "StartupAnimation.h"
#include "EuclideanAnimation.h"

#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")

// ===== COMPILATION CONSTANTS =====
static const unsigned long RENDER_INTERVAL = 50;  // 20 FPS for UI

// ===== GLOBAL OBJECTS =====
// Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// MIDI Interfaces
BluetoothMIDI_Interface ble_midi;
Adafruit_USBD_MIDI usb_midi;

// Controllers
RoutingMatrix routing;
UIController ui;
EuclideanSequencer euclSeq;
Encoder encoder;
EuclideanMidiEngine euclidMidiEngine;
OSCController oscController;

// External
extern MidiClock midiClock;

// ===== APPLICATION STATE =====
enum AppMode {
	MODE_ROUTING,          // MIDI routing menu (initial)
	MODE_SEQUENCER         // Euclidean sequencer
};

AppMode appMode = MODE_ROUTING;
AppMode lastAppMode = appMode;

// ===== WIFI MIDI TASK (Núcleo 1) =====
// Task separada para manter WiFi responsivo
// Não processa fila (evita race conditions), só permite que OSC envie
void wifiMidiTask(void* param) {
	while (true) {
		vTaskDelay(pdMS_TO_TICKS(1));  // 1ms yield
	}
}

// NOTE: MIDI processing moved into EuclideanMidiEngine worker task

// (OSC queue/task removida. Sequencer NÃO envia eventos de playback via OSC; apenas controle/feedback de parâmetros permanece)
bool seqPlaying = false;
bool paramEditMode = false;
volatile bool doubleClickProcessing = false;  // Proteção contra re-entrada

// UI rendering throttling
unsigned long lastRenderTime = 0;
uint8_t lastDisplayedParam = 0xFF;
uint8_t lastDisplayedGroup = 0xFF;

// ===== INITIALIZATION FUNCTIONS =====

void initHardware() {
	// Display and IO
	Wire.begin(OLED_SDA, OLED_SCL);
	u8g2.begin();
	// Mostra animação de inicialização (bloqueante e curta)
	StartupAnimation::show(u8g2);

	// Encoder pins
	pinMode(ENC_CLK, INPUT_PULLUP);
	pinMode(ENC_DT, INPUT_PULLUP);
	pinMode(ENC_SW, INPUT_PULLUP);
}

void initMIDI() {
	// Setup MIDI routing
	MIDIRouter::begin();
	MIDIRouter::setRoutingMatrix(&routing);
	MIDIRouter::setBleInterface(&ble_midi);
	MIDIRouter::setUsbInterface(&usb_midi);
	MIDIRouter::setEuclideanSequencer(&euclSeq);
	MIDIRouter::setMidiClock(&midiClock);
	
	// Initialize USB
	USB.begin();
	delay(1000);
}

void initSequencer() {
	// UI and Input
	routing.clearAll();
	ui.begin(u8g2);
	encoder.begin();
	
	// Core Sequencer
	euclSeq.begin();
	midiClock.begin(120.0);
	midiClock.setTicksPerStep(3);
	
	// MIDI Engine
	euclidMidiEngine.begin(&euclSeq, &midiClock, &ble_midi, &usb_midi);
	
	// Register MidiClock callbacks to route Start/Stop/Clock to selected outputs
	midiClock.setClockCallback(MIDIRouter::clockTickCallback);
	midiClock.setStartCallback(MIDIRouter::startCallback);
	midiClock.setStopCallback(MIDIRouter::stopCallback);

	// Network (OSC over WiFi)
	oscController.setEuclideanSequencer(&euclSeq);
	oscController.setMidiClock(&midiClock);
	oscController.begin();
	OSCMapping::setOSCController(&oscController);
	
	// Registrar clientes P2P manualmente (opcional)
	oscController.findOrAddP2PClient(IPAddress(192, 168, 4, 2), 8000);   // Cliente 1
	oscController.findOrAddP2PClient(IPAddress(192, 168, 4, 4), 8000);   // Cliente 2
	
	// Feedback and Persistence
	MidiFeedback::begin(&usb_midi);
	MidiFeedback::setBluetoothInterface(&ble_midi);
	MidiFeedback::setEuclideanSequencer(&euclSeq);
	
	// Setup callbacks
	euclSeq.onTrackChanged = []() {
		// Callback para track change
	};

	ui.render(routing);
	ble_midi.setName("MIDI_MATRIX_BLE");
	Control_Surface.begin();
	
	// ===== CRIAR TASK WIFI (Núcleo 1) =====
	xTaskCreatePinnedToCore(
		wifiMidiTask,          // Função task
		"WiFiYield",          // Nome (debug)
		2048,                  // Stack size (2KB - mínimo)
		nullptr,               // Parâmetro (não usado)
		1,                     // Prioridade baixa (deixa espaço para WiFi)
		nullptr,               // Handle (não precisa)
		1                      // Núcleo 1
	);

	// A fila MIDI agora é processada internamente pelo EuclideanMidiEngine (worker task)
}

// ===== PARAMETER EDITING =====

void editParam(int8_t direction) {
	switch (euclSeq.getCurrentEditParam()) {
		// PARAM_PRESET removed: presets functionality disabled
		case EuclideanSequencer::PARAM_PLAY: {
			euclSeq.incrementParam(direction);
			
			if (euclSeq.getPlayMode() == EuclideanSequencer::PLAY) {
				midiClock.start();
				seqPlaying = true;
			} else {
				midiClock.stop();
				seqPlaying = false;
			}
			break;
		}
		case EuclideanSequencer::PARAM_TEMPO: {
			// Cicla entre SLAVE e 30-240 BPM
			if (midiClock.isSlave()) {
				// Se está em SLAVE, muda para 120 BPM (padrão)
				midiClock.setSyncMode(MidiClock::MASTER);
				midiClock.setBPM(120.0f);
			} else {
				// Se está em BPM, incrementa/decrementa
				float bpm = midiClock.getBPM();
				float newBpm = bpm + (direction * 5.0f);
				
				// Se tenta ir além dos limites, volta para SLAVE
				if (newBpm < 30.0f || newBpm > 240.0f) {
					midiClock.setSyncMode(MidiClock::SLAVE);
				} else {
					midiClock.setBPM(newBpm);
				}
			}
			break;
		}
		case EuclideanSequencer::PARAM_CLOCK_IO: {
			// Cycle through clock I/O options: usb, ble, din, usb+ble, usb+din, ble+din, none
			{
				uint8_t options[] = {
					MidiClock::CLOCK_USB,
					MidiClock::CLOCK_BLE,
					MidiClock::CLOCK_DIN,
					(uint8_t)(MidiClock::CLOCK_USB | MidiClock::CLOCK_BLE),
					(uint8_t)(MidiClock::CLOCK_USB | MidiClock::CLOCK_DIN),
					(uint8_t)(MidiClock::CLOCK_BLE | MidiClock::CLOCK_DIN),
					MidiClock::CLOCK_NONE
				};
				const int opts = sizeof(options)/sizeof(options[0]);
				uint8_t cur = (uint8_t)midiClock.getClockIO();
				int idx = 0;
				for (int i = 0; i < opts; ++i) if (options[i] == cur) { idx = i; break; }
				idx = (idx + (direction > 0 ? 1 : -1) + opts) % opts;
				midiClock.setClockIO((MidiClock::ClockIO)options[idx]);
			}
			break;
		}
		case EuclideanSequencer::PARAM_NOTE_OUT: {
			// Cycle through note output options: usb, ble, din, usb+ble, usb+din, ble+din, none
			{
				uint8_t options[] = {
					EuclideanSequencer::OUT_USB,
					EuclideanSequencer::OUT_BLE,
					EuclideanSequencer::OUT_DIN,
					(uint8_t)(EuclideanSequencer::OUT_USB | EuclideanSequencer::OUT_BLE),
					(uint8_t)(EuclideanSequencer::OUT_USB | EuclideanSequencer::OUT_DIN),
					(uint8_t)(EuclideanSequencer::OUT_BLE | EuclideanSequencer::OUT_DIN),
					0x00
				};
				const int opts = sizeof(options)/sizeof(options[0]);
				uint8_t cur = (uint8_t)euclSeq.getOutputNotes();
				int idx = 0;
				for (int i = 0; i < opts; ++i) if (options[i] == cur) { idx = i; break; }
				idx = (idx + (direction > 0 ? 1 : -1) + opts) % opts;
				euclSeq.setOutputNotes((EuclideanSequencer::OutputProtocol)options[idx]);
			}
			break;
		}
		case EuclideanSequencer::PARAM_MIDI_MAP: {
			// Toggle MidiMap ON/OFF
			{
				bool cur = euclSeq.getOutputMidiMap();
				euclSeq.setOutputMidiMap(!cur);
			}
			break;
		}
		case EuclideanSequencer::PARAM_OSC_MAP: {
			// Toggle OSCMap ON/OFF
			{
				bool cur = euclSeq.getOutputOSCMap();
				euclSeq.setOutputOSCMap(!cur);
			}
			break;
		}
		case EuclideanSequencer::PARAM_NOTE:
		case EuclideanSequencer::PARAM_VELOCITY:
		case EuclideanSequencer::PARAM_MIDI_CHANNEL:
			euclSeq.incrementParam(direction);
			break;
		default: {
			if (direction > 0) {
				euclSeq.incrementParam(direction);
			} else {
				euclSeq.decrementParam(-direction);
			}
			break;
		}
	}
}

// ===== INPUT HANDLING =====

void handleEncoderInput() {
	Encoder::ClickType clickType = encoder.readClickType();
	int rotation = encoder.readRotation();

	if (appMode == MODE_SEQUENCER) {
		if (clickType == Encoder::CLICK_LONG) {
			appMode = MODE_ROUTING;
			return;
		}

		if (clickType == Encoder::CLICK_BACKWARD) {
			euclSeq.nextGroup();
			paramEditMode = false;
			return;
		}

		if (clickType == Encoder::CLICK_FORWARD) {
			paramEditMode = !paramEditMode;
			return;
		}

		if (rotation == 0) return;

		if (!paramEditMode) {
			if (rotation > 0) {
				euclSeq.nextEditParam();
			} else {
				euclSeq.prevEditParam();
			}
		} else {
			editParam(rotation);
		}
		return;
	}

	if (appMode == MODE_ROUTING) {
		if (clickType == Encoder::CLICK_LONG) {
			euclSeq.begin();
			appMode = MODE_SEQUENCER;
			// Mostrar animação euclidiana ao entrar no sequenciador
			EuclideanAnimation::show(u8g2);
			paramEditMode = false;
			midiClock.reset();
			// Enviar snapshot único de parâmetros por MIDI e OSC ao entrar no modo sequenciador
			MidiFeedback::sendAllFeedbackForced(&euclSeq, &midiClock);
			OSCMapping::sendAllFeedbackForced(&euclSeq, &midiClock);
			return;
		}

		if (rotation != 0) {
			ui.processEncoderRotation(rotation);
		}
	}
}

void handleSerialMidi() {
	HardwareSerial* serials[] = {&Serial1, &Serial2, &Serial};
	for (uint8_t i = 0; i < 3; ++i) {
		while (serials[i]->available()) {
			uint8_t b = serials[i]->read();
			// Real-time messages (0xF8..0xFF) — tratar centralizadamente
			if (b >= 0xF8) {
				MIDIRouter::routeRealTimeMessage(i, b);
				continue;
			}
			MIDIRouter::routeByteFromInput(i, b);
		}
	}
}

void handleUsbMidi() {
	while (usb_midi.available()) {
		uint8_t status = usb_midi.read();
		
		if (status >= 0xF8) {
			MIDIRouter::routeRealTimeMessage(4, status);
			continue;
		}
		
		if (!usb_midi.available()) break;
		uint8_t data1 = usb_midi.read();
		
		if (!usb_midi.available()) break;
		uint8_t data2 = usb_midi.read();
		
		MIDIRouter::routeChannelMessage(4, status, data1, data2);
	}
}

void handleBLEMidi() {
	MIDIReadEvent evt;
	while ((evt = ble_midi.read()) != MIDIReadEvent::NO_MESSAGE) {
		if (evt == MIDIReadEvent::CHANNEL_MESSAGE) {
			auto msg = ble_midi.getChannelMessage();
			MIDIRouter::routeChannelMessage(3, msg.header, msg.data1, msg.data2);
		} else if (evt == MIDIReadEvent::REALTIME_MESSAGE) {
			auto msg = ble_midi.getRealTimeMessage();
			MIDIRouter::routeRealTimeMessage(3, msg.message);
		}
	}
}

void handleAllMidiInput() {
	for (uint8_t round = 0; round < 3; ++round) {
		handleBLEMidi();
		Control_Surface.loop();
		handleSerialMidi();
		handleUsbMidi();
	}
}

// ===== MIDI CALLBACKS (from MidiCCMapping) =====

extern "C" {
	void midiEncoderRotation(int8_t direction) {
		if (appMode != MODE_SEQUENCER) return;
		
		if (!paramEditMode) {
			if (direction > 0) {
				euclSeq.nextEditParam();
			} else {
				euclSeq.prevEditParam();
			}
		} else {
			editParam(direction);
		}
	}
    void midiEncoderDoubleClick() {
		if (appMode != MODE_SEQUENCER) return;
		if (doubleClickProcessing) return;  // Evita re-entrada
		doubleClickProcessing = true;
		euclSeq.nextGroup();
		paramEditMode = false;
		// Enviar feedback MIDI: double click note
		MidiFeedback::sendNote(MidiCCMapping::getNoteEncoderDoubleClick(), 127);
		doubleClickProcessing = false;
	}
	
	void midiEncoderLongPress() {
		if (appMode == MODE_ROUTING) {
			euclSeq.begin();
			appMode = MODE_SEQUENCER;
			paramEditMode = false;
			midiClock.reset();
			// Mostrar animação euclidiana ao entrar no sequenciador
			EuclideanAnimation::show(u8g2);
			// Enviar snapshot único de parâmetros por MIDI e OSC ao entrar no modo sequenciador
			MidiFeedback::sendAllFeedbackForced(&euclSeq, &midiClock);
			OSCMapping::sendAllFeedbackForced(&euclSeq, &midiClock);
		} else if (appMode == MODE_SEQUENCER) {
			appMode = MODE_ROUTING;
		}
		// Enviar feedback MIDI: long press note
		MidiFeedback::sendNote(MidiCCMapping::getNoteEncoderLongPress(), 127);
	}
	
	void midiRoutingMatrixToggle(uint8_t inIndex, uint8_t outIndex) {
		routing.toggle(inIndex, outIndex);
		// Feedback via MIDI note mapping removido; a matriz é apenas alternada localmente
	}
	
	// Preset MIDI callbacks removed
}

// ===== UI RENDERING =====

void handleUIRendering() {
	unsigned long now = millis();
	bool needsRender = false;
	
	if (appMode == MODE_SEQUENCER) {
		uint8_t currentParam = euclSeq.getCurrentEditParam();
		uint8_t currentGroup = euclSeq.getCurrentGroup();
		if (currentParam != lastDisplayedParam || currentGroup != lastDisplayedGroup || 
		    (now - lastRenderTime) >= RENDER_INTERVAL) {
			needsRender = true;
			lastDisplayedParam = currentParam;
			lastDisplayedGroup = currentGroup;
			lastRenderTime = now;
		}
		
		if (needsRender) {
			ui.renderEuclidean(euclSeq, midiClock, paramEditMode);
		}
	} else {
		ui.render(routing);
		ui.handleInput(routing);
		lastDisplayedParam = 0xFF;
	}
}

// ===== SETUP & LOOP =====

void setup() {
	initHardware();
    initMIDI();
    initSequencer();
}

void loop() {
	// Prioridade 1: Processar engine MIDI (notas, timing crítico)
	euclidMidiEngine.update();

	// Processar envios pendentes gerados pelo ISR do MidiClock (envio seguro de Start/Stop/Clock)
	// If a dedicated clock task exists, it will process pending realtime events.
	// Otherwise, process them here in the main loop for compatibility.
	if (!midiClock.clockTaskHandle) {
		midiClock.processPendingRealTime();
	}

	// Detectar transição de modo e enviar snapshot único ao entrar em SEQUENCER
	if (lastAppMode != appMode) {
		if (appMode == MODE_SEQUENCER) {
			MidiFeedback::sendAllFeedback(&euclSeq, &midiClock);
			OSCMapping::sendAllFeedback(&euclSeq, &midiClock);
		}
		lastAppMode = appMode;
	}

	// (Removido: processMidiQueue agora roda em task dedicada)

	// Prioridade 2: Feedback MIDI e OSC (status updates)
	MidiFeedback::sendAllFeedback(&euclSeq, &midiClock);
	OSCMapping::sendAllFeedback(&euclSeq, &midiClock);

	// Prioridade 3: Input (interrupções já ligadas, só processar)
	handleAllMidiInput();
	handleEncoderInput();

	// Prioridade 4: UI (menos crítico, throttled)
	handleUIRendering();

	// Permitir outras tarefas do sistema (WiFi, etc) com pequeno yield
	delay(1);  // 1ms para evitar watchdog timeout
}
