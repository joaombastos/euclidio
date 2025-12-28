#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
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
#include "EuclideanHarmonicSequencer.h"
#include "MidiFeedback.h"
#include "MidiCCMapping.h"
#include "OSCController.h"
#include "OSCMapping.h"
#include "StartupAnimation.h"
#include "EuclideanAnimation.h"
#include "SdCard.h"
#include "PresetManager.h"
#include "PresetUI.h"

#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")

// ===== COMPILATION CONSTANTS =====
static const unsigned long RENDER_INTERVAL = 50;  // 20 FPS for UI

// ===== GLOBAL OBJECTS =====
// Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// MIDI Interfaces
Adafruit_USBD_MIDI usb_midi;

// Controllers
RoutingMatrix routing;
UIController ui;
EuclideanSequencer euclSeq;
Encoder encoder;
EuclideanMidiEngine euclidMidiEngine;
EuclideanHarmonicSequencer harmonicSeq;
OSCController oscController;

// Instância global de MidiClock
MidiClock midiClock;

// ===== APPLICATION STATE =====
#include "AppState.h"

AppMode appMode = MODE_ROUTING;
AppMode lastAppMode = appMode;
bool g_presetMenuActive = false;

// ===== WIFI MIDI TASK (Núcleo 1) =====
// Task separada para manter WiFi responsivo
// Não processa fila (evita race conditions), só permite que OSC envie
void wifiMidiTask(void* param) {
	while (true) {
		vTaskDelay(pdMS_TO_TICKS(1));  // 1ms yield
	}
}
bool seqPlaying = false;
bool paramEditMode = false;
volatile bool doubleClickProcessing = false;  // Proteção contra re-entrada
volatile bool oscEncoderDoubleClickRequested = false; // Request set by OSC handler, processed in main loop
uint8_t harmonicEditParam = 0; // Parâmetro selecionado
bool harmonicChordEditMode = false;
uint8_t harmonicChordEditIndex = 0;
std::vector<uint8_t> harmonicAllowedDegrees;

// UI rendering throttling
unsigned long lastRenderTime = 0;
uint8_t lastDisplayedParam = 0xFF;
uint8_t lastDisplayedGroup = 0xFF;
// Throttle feedback to avoid blocking (ms)
static const unsigned long FEEDBACK_INTERVAL_MS = 200;
unsigned long lastFeedbackTime = 0;

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
	MIDIRouter::setUsbInterface(&usb_midi);
	MIDIRouter::setEuclideanSequencer(&euclSeq);
	MIDIRouter::setMidiClock(&midiClock);
	
	// Initialize USB
	USB.begin();
	// Aguarda USB estar montado (pronto), timeout de 1s
	unsigned long usbStart = millis();
	while (!tud_mounted() && (millis() - usbStart < 1000)) {
		yield();
	}
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
	euclidMidiEngine.begin(&euclSeq, &midiClock, &usb_midi);

	// Harmonic sequencer (não inicia por padrão)
	harmonicSeq.begin(&euclidMidiEngine, &midiClock);
	// Garante pelo menos um acorde na lista para navegação inicial
	if (harmonicSeq.getChordListSize() == 0) {
		harmonicSeq.addChordDegree(0);
	}
	
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
	MidiFeedback::setEuclideanSequencer(&euclSeq);

	euclSeq.onTrackChanged = []() {
		// Snapshot forçado ao trocar de track: garante defaults visíveis
		MidiFeedback::sendAllFeedbackForced(&euclSeq, &midiClock);
		OSCMapping::sendAllFeedbackForced(&euclSeq, &midiClock);
	};

	ui.render(routing);
	// BLE removed
	// Control_Surface.begin() removed (library eliminated)
	
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
}

// ===== PARAMETER EDITING =====
// Parâmetros do harmônico: 0-14 (parâmetros principais, inclusive `Oct`), 15+ (acordes)

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
				float newBpm = bpm + (direction * 1.0f);
				
				// Se tenta ir além dos limites, volta para SLAVE
				if (newBpm < 30.0f || newBpm > 240.0f) {
					midiClock.setSyncMode(MidiClock::SLAVE);
				} else {
					midiClock.setBPM(newBpm);
				}
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
	EncoderHandler_handle();
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
	// BLE MIDI input removed: Control Surface library eliminated.
	// This function intentionally left empty.
}
void handleAllMidiInput() {
	for (uint8_t round = 0; round < 3; ++round) {
		handleBLEMidi();
		// Control_Surface.loop() removed (library eliminated)
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
		if (doubleClickProcessing) return;  // Evita re-entrada
		doubleClickProcessing = true;
		if (appMode == MODE_SEQUENCER) {
			// Sempre trocar para modo harmônico ao duplo clique OSC
			appMode = MODE_HARMONIC;
			paramEditMode = false;
			// Send forced snapshot of harmonic sequencer parameters on first entry
			MidiFeedback::sendAllHarmonicFeedbackForced(&harmonicSeq, &midiClock);
			OSCMapping::sendAllHarmonicFeedbackForced(&harmonicSeq, &midiClock);
		} else if (appMode == MODE_HARMONIC) {
			// return to sequencer
			appMode = MODE_SEQUENCER;
			paramEditMode = false;
			// Ensure we return to the rhythmic sequencer Group 1 (index 0)
			MidiFeedback::sendAllFeedback(&euclSeq, &midiClock);
			OSCMapping::sendAllFeedback(&euclSeq, &midiClock);
		}
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
			// Menu de carregamento de preset (antes da animação)
			if (PresetUI::showLoadMenu(u8g2)) {
				// User selected "Load" - show preset selection
				String presetName = PresetUI::selectPreset(u8g2, false);
				if (presetName.length() > 0) {
					PresetManager::loadEuclideanPreset(&euclSeq, presetName.c_str());
				}
			}
			// Mostrar animação euclidiana ao entrar no sequenciador
			EuclideanAnimation::show(u8g2);
			// Enviar snapshot único de parâmetros por MIDI e OSC ao entrar no modo sequenciador
			MidiFeedback::sendAllFeedbackForced(&euclSeq, &midiClock);
			OSCMapping::sendAllFeedbackForced(&euclSeq, &midiClock);
			// Também enviar snapshot forçado do sequenciador harmônico logo na entrada
			MidiFeedback::sendAllHarmonicFeedbackForced(&harmonicSeq, &midiClock);
			OSCMapping::sendAllHarmonicFeedbackForced(&harmonicSeq, &midiClock);
		} else if (appMode == MODE_SEQUENCER || appMode == MODE_HARMONIC) {
			// Show Save/No menu when exiting sequencer modes
			bool shouldSave = PresetUI::showSaveMenu(u8g2);
			if (shouldSave) {
				// Get preset name from user
				String presetName = PresetUI::selectPreset(u8g2, true);
				if (presetName.length() > 0) {
					PresetManager::saveEuclideanPreset(&euclSeq, presetName.c_str());
					PresetManager::saveHarmonicPreset(&harmonicSeq, presetName.c_str());
				}
			}
			// Reset sequencers to defaults and stop playback when exiting to routing
			euclSeq.begin();
			euclSeq.stop();
			harmonicSeq.resetToDefaults();
			harmonicSeq.stop();
			midiClock.stop();
			appMode = MODE_ROUTING;
		}
		// Enviar feedback MIDI: long press note
		MidiFeedback::sendNote(MidiCCMapping::getNoteEncoderLongPress(), 127);
	}
	
	void midiRoutingMatrixToggle(uint8_t inIndex, uint8_t outIndex) {
		routing.toggle(inIndex, outIndex);
		// Feedback via MIDI note mapping removido; a matriz é apenas alternada localmente
	}
}
// ===== UI RENDERING =====
void handleUIRendering() {
	unsigned long now = millis();
	bool needsRender = false;
	
	if (appMode == MODE_SEQUENCER) {
		uint8_t currentParam = euclSeq.getCurrentEditParam();
		if (currentParam != lastDisplayedParam || (now - lastRenderTime) >= RENDER_INTERVAL) {
			needsRender = true;
			lastDisplayedParam = currentParam;
			lastRenderTime = now;
		}
		if (needsRender) {
			ui.renderEuclidean(euclSeq, midiClock, paramEditMode);
		}
	} else if (appMode == MODE_HARMONIC) {
		// Render harmonic UI
		uint8_t currentParam = harmonicEditParam;
		uint8_t currentGroup = 0;
		if ((now - lastRenderTime) >= RENDER_INTERVAL || currentParam != lastDisplayedParam) {
			needsRender = true;
			lastDisplayedParam = currentParam;
			lastDisplayedGroup = currentGroup;
			lastRenderTime = now;
		}
		if (needsRender) ui.renderHarmonic(harmonicSeq, midiClock, paramEditMode, harmonicEditParam, harmonicChordEditMode, harmonicChordEditIndex);
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
    
    // Inicializar SD e criar presets padrão se não existirem
    PresetManager::initDirectories();
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
	// Prioridade 2: (removido) Feedback MIDI e OSC periódico global ao iniciar.
	// O feedback contínuo agora é responsabilidade apenas dos modos/sequenciadores
	// (snapshots e updates específicos já existentes em MidiFeedback/OSCMapping).
	// Prioridade 3: Input (interrupções já ligadas, só processar)
	handleAllMidiInput();
	// Process deferred OSC-initiated encoder double-clicks in main context
	if (oscEncoderDoubleClickRequested) {
		oscEncoderDoubleClickRequested = false;
		midiEncoderDoubleClick();
	}
	handleEncoderInput();
	// Prioridade 4: UI (menos crítico, throttled)
	handleUIRendering();
	// Permitir outras tarefas do sistema (WiFi, etc) com pequeno yield
	yield();  // yield para evitar watchdog timeout e melhorar responsividade
}
