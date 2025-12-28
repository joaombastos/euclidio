#include "OSCMapping.h"
#include "EuclideanSequencer.h"
#include "MidiClock.h"
#include "OSCController.h"
#include "RoutingMatrix.h"
#include <Arduino.h>
#include <string.h>
#include <OSCMessage.h>
#include "EuclideanHarmonicSequencer.h"
// MIDI feedback helpers
#include "MidiFeedback.h"
#include "AppState.h"

// access to harmonic sequencer global (defined in main.cpp)
extern EuclideanHarmonicSequencer harmonicSeq;
extern uint8_t harmonicChordEditIndex;

// Forward declarations para callbacks (definidos em main.cpp)
extern "C" {
	void midiEncoderDoubleClick();
	void midiEncoderLongPress();
	void midiRoutingMatrixToggle(uint8_t inIndex, uint8_t outIndex);
}

const char* OSCMapping::PATH_STEPS = "/sequencer/steps";
const char* OSCMapping::PATH_HITS = "/sequencer/hits";
const char* OSCMapping::PATH_OFFSET = "/sequencer/offset";
const char* OSCMapping::PATH_NOTE = "/sequencer/note";
const char* OSCMapping::PATH_VELOCITY = "/sequencer/velocity";
const char* OSCMapping::PATH_CHANNEL = "/sequencer/channel";
const char* OSCMapping::PATH_RESOLUTION = "/sequencer/resolution";
const char* OSCMapping::PATH_TRACK = "/sequencer/track";
const char* OSCMapping::PATH_PLAYSTOP = "/sequencer/playstop";
const char* OSCMapping::PATH_TEMPO = "/sequencer/tempo";
const char* OSCMapping::PATH_NOTE_LENGTH = "/sequencer/note_length";
// Per-track base path (clients should use "/sequencer/dub/<n>")
const char* OSCMapping::PATH_DUB_BASE = "/sequencer/dub/";
const char* OSCMapping::PATH_ENCODER_DOUBLE_CLICK = "/encoder/double_click";
const char* OSCMapping::PATH_ENCODER_LONG_PRESS = "/encoder/long_press";

// Harmonic paths
const char* OSCMapping::PATH_HARMONIC_BASE = "/harmonic";
const char* OSCMapping::PATH_HARMONIC_TONALITY = "/harmonic/tonality";
const char* OSCMapping::PATH_HARMONIC_SCALE = "/harmonic/scale";
const char* OSCMapping::PATH_HARMONIC_SCALE_NAME = "/harmonic/scale/name";
const char* OSCMapping::PATH_HARMONIC_MODE = "/harmonic/mode";
const char* OSCMapping::PATH_HARMONIC_STEPS = "/harmonic/steps";
const char* OSCMapping::PATH_HARMONIC_HITS = "/harmonic/hits";
const char* OSCMapping::PATH_HARMONIC_OFFSET = "/harmonic/offset";
const char* OSCMapping::PATH_HARMONIC_POLY = "/harmonic/poly";
const char* OSCMapping::PATH_HARMONIC_VELOCITY = "/harmonic/velocity";
const char* OSCMapping::PATH_HARMONIC_NOTE_LENGTH = "/harmonic/note_length";
const char* OSCMapping::PATH_HARMONIC_OCTAVE = "/harmonic/octave";
const char* OSCMapping::PATH_HARMONIC_ACTIVE = "/harmonic/active";
const char* OSCMapping::PATH_HARMONIC_TRACK = "/harmonic/track";
const char* OSCMapping::PATH_HARMONIC_RESOLUTION = "/harmonic/resolution";
const char* OSCMapping::PATH_HARMONIC_CHANNEL = "/harmonic/channel";
const char* OSCMapping::PATH_HARMONIC_CHORDS_COUNT = "/harmonic/chords/count";

// Chord list paths
const char* OSCMapping::PATH_HARMONIC_CHORDS_SELECT = "/harmonic/chords/select";
const char* OSCMapping::PATH_HARMONIC_CHORDS_SET = "/harmonic/chords/set";
const char* OSCMapping::PATH_HARMONIC_CHORDS_INSERT = "/harmonic/chords/insert";
const char* OSCMapping::PATH_HARMONIC_CHORDS_DELETE = "/harmonic/chords/delete";
const char* OSCMapping::PATH_HARMONIC_CHORDS_TOGGLE = "/harmonic/chords/toggle";


const char* OSCMapping::PATH_ROUTING_TOGGLE = "/routing/toggle";


int OSCMapping::mapFloatToInt(float value, int min, int max) {
	return constrain((int)value, min, max);
}

uint8_t OSCMapping::mapFloatToUint8(float value, uint8_t min, uint8_t max) {
	return constrain((uint8_t)value, min, max);
}

void OSCMapping::processMessage(const char* path, int argc, float* argv, EuclideanSequencer* seq, MidiClock* clock) {
	if (!seq) return;
    
	// Compara paths e processa comandos
	if (strcmp(path, PATH_STEPS) == 0) {
		if (argc >= 1) {
			seq->setSteps(mapFloatToInt(argv[0], 1, 32));
		}
	} else if (strcmp(path, PATH_HITS) == 0) {
		if (argc >= 1) {
			seq->setHits(mapFloatToInt(argv[0], 1, 32));
		}
	} else if (strcmp(path, PATH_OFFSET) == 0) {
		if (argc >= 1) {
			seq->setOffset(mapFloatToUint8(argv[0], 0, 31));
		}
	} else if (strcmp(path, PATH_NOTE) == 0) {
		if (argc >= 1) {
			seq->setNote(mapFloatToUint8(argv[0], 0, 127));
		}
	} else if (strcmp(path, PATH_VELOCITY) == 0) {
		if (argc >= 1) {
			seq->setVelocity(mapFloatToUint8(argv[0], 0, 127));
		}
	} else if (strcmp(path, PATH_CHANNEL) == 0) {
		if (argc >= 1) {
			seq->setMidiChannel(mapFloatToUint8(argv[0], 0, 15));
		}
	} else if (strcmp(path, PATH_RESOLUTION) == 0) {
		if (argc >= 1) {
			seq->setResolution(mapFloatToInt(argv[0], 1, 4));
		}
	} else if (strcmp(path, PATH_TRACK) == 0) {
		if (argc >= 1) {
			seq->setSelectedPattern(mapFloatToUint8(argv[0], 0, 7));
		}
	} else if (strcmp(path, PATH_PLAYSTOP) == 0) {
		// Mensagem combinada: /sequencer/playstop [start] [stop]
		// start: valor > 0 => start, stop: valor > 0 => stop
		// Se ambos presentes, o comando de stop tem prioridade (processado por último).
		if (argc >= 1) {
			if (argv[0] > 0) {
				// Atualiza também o modo de play para que a UI mostre "Play"
				seq->setPlayMode(EuclideanSequencer::PLAY);
				if (clock) {
					clock->start();
					seq->start();
				} else {
					seq->start();
				}
			}
		}
		if (argc >= 2) {
			if (argv[1] > 0) {
				// Atualiza também o modo de play para que a UI mostre "Stop"
				seq->setPlayMode(EuclideanSequencer::STOP);
				if (clock) {
					clock->stop();
					seq->stop();
				} else {
					seq->stop();
				}
			}
		}
	} else if (strcmp(path, PATH_TEMPO) == 0) {
		if (clock && argc >= 1) {
			if (argv[0] < 0) {
				// Valor negativo = modo SLAVE
				clock->setSyncMode(MidiClock::SLAVE);
			} else {
				clock->setSyncMode(MidiClock::MASTER);
				float bpm = constrain(argv[0], 30.0f, 240.0f);
				clock->setBPM(bpm);
			}
		}
	} else if (strcmp(path, PATH_NOTE_LENGTH) == 0) {
		if (argc >= 1) {
			uint16_t length = mapFloatToInt(argv[0], 50, 700);
			seq->setNoteLength(length);
		}
	} else if (strncmp(path, PATH_DUB_BASE, strlen(PATH_DUB_BASE)) == 0) {
		// Expect path like /sequencer/dub/<n>
		const char* suffix = path + strlen(PATH_DUB_BASE);
		if (suffix && *suffix) {
			int idx = atoi(suffix);
			if (idx >= 0 && idx < 8) {
				uint8_t trackIdx = (uint8_t)idx;
				bool enabled = seq->isTrackEnabled(trackIdx);
				seq->setTrackEnabled(trackIdx, !enabled);
			}
		}
	} else if (strcmp(path, PATH_ENCODER_DOUBLE_CLICK) == 0) {
		// Defer handling to main loop to avoid concurrent state changes from UDP task
		oscEncoderDoubleClickRequested = true;
	} else if (strcmp(path, PATH_ENCODER_LONG_PRESS) == 0) {
		// Long press is less frequent, but keep immediate handling for now
		midiEncoderLongPress();
	}
	// Harmonic sequencer OSC handling
	else if (strncmp(path, "/harmonic", 9) == 0) {
		// /harmonic/... routes
		if (strcmp(path, PATH_HARMONIC_TONALITY) == 0) {
			if (argc >= 1) harmonicSeq.setTonic((uint8_t)constrain((int)argv[0], 0, 11));
		} else if (strcmp(path, PATH_HARMONIC_SCALE) == 0) {
			if (argc >= 1) {
				int idx = constrain((int)argv[0], 0, (int)EuclideanHarmonicSequencer::SCALE_COUNT - 1);
				harmonicSeq.setScaleType((EuclideanHarmonicSequencer::ScaleType)idx);
			}
		} else if (strcmp(path, PATH_HARMONIC_MODE) == 0) {
			if (argc >= 1) harmonicSeq.setDistributionMode((EuclideanHarmonicSequencer::DistributionMode)(argv[0] >= 1.0f));
		} else if (strcmp(path, PATH_HARMONIC_STEPS) == 0) {
			if (argc >= 1) harmonicSeq.setSteps((uint8_t)constrain((int)argv[0], 1, 32));
		} else if (strcmp(path, PATH_HARMONIC_HITS) == 0) {
			if (argc >= 1) harmonicSeq.setHits((uint8_t)constrain((int)argv[0], 1, 32));
		} else if (strcmp(path, PATH_HARMONIC_OFFSET) == 0) {
			if (argc >= 1) harmonicSeq.setOffset((uint8_t)constrain((int)argv[0], 0, 31));
		} else if (strcmp(path, PATH_HARMONIC_POLY) == 0) {
			if (argc >= 1) harmonicSeq.setPolyphony((uint8_t)constrain((int)argv[0], 1, 8));
		} else if (strcmp(path, PATH_HARMONIC_VELOCITY) == 0) {
			if (argc >= 1) harmonicSeq.setVelocity((uint8_t)constrain((int)argv[0], 0, 127));
		} else if (strcmp(path, PATH_HARMONIC_NOTE_LENGTH) == 0) {
			if (argc >= 1) harmonicSeq.setNoteLength((uint16_t)constrain((int)argv[0], 10, 2000));
		} else if (strcmp(path, PATH_HARMONIC_OCTAVE) == 0) {
			if (argc >= 1) {
				int oct = (int)constrain((int)argv[0], -2, 2);
				harmonicSeq.setBaseOctave((int8_t)oct);
			}
		} else if (strcmp(path, PATH_HARMONIC_ACTIVE) == 0) {
			if (argc >= 1) harmonicSeq.setActive(argv[0] != 0.0f);
		}
		// Per-track active: /harmonic/active/<n>  value: 0 = off, >0 = on
		else if (strncmp(path, "/harmonic/active/", 17) == 0) {
			const char* suffix = path + 17;
			if (suffix && *suffix && argc >= 1) {
				int idx = atoi(suffix); // expected 1..MAX_TRACKS (one-based)
				if (idx >= 1 && idx <= (int)EuclideanHarmonicSequencer::MAX_TRACKS) {
					int prev = (int)harmonicSeq.getActiveTrackNumber();
					harmonicSeq.setActiveTrack((uint8_t)idx);
					harmonicSeq.setActive(argv[0] != 0.0f);
					// restore previous active track
					harmonicSeq.setActiveTrack((uint8_t)prev);
				}
			}
		}
		// Harmonic track select (1..MAX_TRACKS)
		else if (strcmp(path, PATH_HARMONIC_TRACK) == 0) {
			if (argc >= 1) {
				int t = constrain((int)argv[0], 1, (int)EuclideanHarmonicSequencer::MAX_TRACKS);
				harmonicSeq.setActiveTrack((uint8_t)t);
			}
		}
		// MIDI channel for harmonic sequencer
		else if (strcmp(path, PATH_HARMONIC_CHANNEL) == 0) {
			if (argc >= 1) {
				int ch = constrain((int)argv[0], 0, 15);
				harmonicSeq.setMidiChannel((uint8_t)ch);
			}
		}
		// Harmonic resolution (index: 0=1/4, 1=1/8, 2=1/16)
		else if (strcmp(path, PATH_HARMONIC_RESOLUTION) == 0) {
			if (argc >= 1) {
				int idx = constrain((int)argv[0], 0, 2);
				harmonicSeq.setResolutionIndex((uint8_t)idx);
			}
		}
		// Chord list operations
		else if (strcmp(path, PATH_HARMONIC_CHORDS_SELECT) == 0) {
			if (argc >= 1) harmonicChordEditIndex = (uint8_t)constrain((int)argv[0], 0, 31);
		} else if (strcmp(path, PATH_HARMONIC_CHORDS_SET) == 0) {
			if (argc >= 2) {
				int idx = (int)argv[0];
				int deg = (int)argv[1];
				if (idx >= 0) harmonicSeq.setChordListItem((uint8_t)idx, (uint8_t)constrain(deg, 0, 6));
			}
		} else if (strcmp(path, PATH_HARMONIC_CHORDS_INSERT) == 0) {
			if (argc >= 2) {
				int idx = (int)argv[0];
				int deg = (int)argv[1];
				if (idx >= 0) harmonicSeq.insertChordAt((uint8_t)idx, (uint8_t)constrain(deg, 0, 6));
			}
		} else if (strcmp(path, PATH_HARMONIC_CHORDS_DELETE) == 0) {
			if (argc >= 1) harmonicSeq.removeChordAt((uint8_t)constrain((int)argv[0], 0, 15));
		} else if (strcmp(path, PATH_HARMONIC_CHORDS_TOGGLE) == 0) {
			if (argc >= 1) {
				int idx = (int)argv[0];
				if (idx >= 0 && idx < 16) {
					// Toggle: if slot exists, remove; otherwise insert degree 0
					if (idx < (int)harmonicSeq.getChordListSize()) {
						harmonicSeq.removeChordAt((uint8_t)idx);
					} else {
						harmonicSeq.insertChordAt((uint8_t)idx, 0);
					}
				}
			}
		}
		else if (strcmp(path, PATH_HARMONIC_CHORDS_COUNT) == 0) {
			// Adjust the number of chord slots for the active track
			if (argc >= 1) {
				int desired = constrain((int)argv[0], 0, 16); // allow 0..16
				uint8_t cur = harmonicSeq.getChordListSize();
				if (desired > (int)cur) {
					// add default degrees (incremental) until desired
					uint8_t lastDeg = (cur > 0) ? harmonicSeq.getChordListItem(cur - 1) : 0;
					for (int i = cur; i < desired; ++i) {
						lastDeg = (lastDeg + 1) % 7;
						harmonicSeq.addChordDegree(lastDeg);
					}
				} else if (desired < (int)cur) {
					// remove from end
					for (int i = cur; i > desired; --i) {
						harmonicSeq.removeLastChord();
					}
				}
				// send forced feedback so controllers/UI update immediately
				MidiFeedback::sendAllHarmonicFeedbackForced(&harmonicSeq, clock);
				OSCMapping::sendAllHarmonicFeedbackForced(&harmonicSeq, clock);
			}
		}
		// Support singular chord path used by some clients: /harmonic/chord/<n> [degree]
		else if (strncmp(path, "/harmonic/chord/", 16) == 0) {
			const char* suffix = path + 16;
			if (suffix && *suffix) {
				int idx = atoi(suffix);
				if (idx >= 0) {
					if (argc >= 1) {
						int deg = (int)argv[0];
						deg = constrain(deg, 0, 6);
						if (idx < (int)harmonicSeq.getChordListSize()) {
							harmonicSeq.setChordListItem((uint8_t)idx, (uint8_t)deg);
						} else {
							harmonicSeq.insertChordAt((uint8_t)idx, (uint8_t)deg);
						}
					} else {
						// No arg: toggle existence
						if (idx < (int)harmonicSeq.getChordListSize()) {
							harmonicSeq.removeChordAt((uint8_t)idx);
						} else {
							harmonicSeq.insertChordAt((uint8_t)idx, 0);
						}
					}
				}
			}
		}
		// Remove stray closing brace here
	} 
	else if (strncmp(path, "/routing/", 9) == 0) {
		// Copiar path para tokenizar
		char buf[64];
		strncpy(buf, path, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		char* tok = strtok(buf, "/"); // "routing"
		char* src = strtok(NULL, "/");
		char* dst = strtok(NULL, "/");
		if (src && dst) {
			// Mapear source para índice: in1=0,in2=1,in3=2,ble=3,usb=4
			int inIndex = -1;
			if (strcmp(src, "in1") == 0) inIndex = 0;
			else if (strcmp(src, "in2") == 0) inIndex = 1;
			else if (strcmp(src, "in3") == 0) inIndex = 2;
			else if (strcmp(src, "ble") == 0) inIndex = 3;
			else if (strcmp(src, "usb") == 0) inIndex = 4;

			// Mapear dest para índice (somente out1..out3 suportados aqui): out1=0,out2=1,out3=2
			int outIndex = -1;
			if (strcmp(dst, "out1") == 0) outIndex = 0;
			else if (strcmp(dst, "out2") == 0) outIndex = 1;
			else if (strcmp(dst, "out3") == 0) outIndex = 2;

			if (inIndex >= 0 && outIndex >= 0) {
				midiRoutingMatrixToggle((uint8_t)inIndex, (uint8_t)outIndex);
			}
		}
	}
}

// Instâncias estáticas
OSCController* OSCMapping::oscController = nullptr;
uint8_t OSCMapping::lastSteps = 0xFF;
uint8_t OSCMapping::lastHits = 0xFF;
uint8_t OSCMapping::lastOffset = 0xFF;
uint8_t OSCMapping::lastNote = 0xFF;
uint8_t OSCMapping::lastVelocity = 0xFF;
uint8_t OSCMapping::lastChannel = 0xFF;
uint8_t OSCMapping::lastResolution = 0xFF;
uint8_t OSCMapping::lastTrack = 0xFF;
bool OSCMapping::lastPlayState = false;
float OSCMapping::lastTempo = -1.0f;
bool OSCMapping::lastDubState[8] = {false, false, false, false, false, false, false, false};
uint16_t OSCMapping::lastNoteLength = 0xFFFF;

// Harmonic caches
int32_t OSCMapping::lastHarmonicTonality = -1;
int32_t OSCMapping::lastHarmonicScale = -1;
int32_t OSCMapping::lastHarmonicMode = -1;
int32_t OSCMapping::lastHarmonicSteps = -1;
int32_t OSCMapping::lastHarmonicHits = -1;
int32_t OSCMapping::lastHarmonicOffset = -1;
int32_t OSCMapping::lastHarmonicPoly = -1;
int32_t OSCMapping::lastHarmonicVelocity = -1;
int32_t OSCMapping::lastHarmonicNoteLength = -1;
int32_t OSCMapping::lastHarmonicActive = -1;
int32_t OSCMapping::lastHarmonicChordSlot[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
int32_t OSCMapping::lastHarmonicTrackActive[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
int32_t OSCMapping::lastHarmonicTrack = -1;
int32_t OSCMapping::lastHarmonicResolution = -1;
int32_t OSCMapping::lastHarmonicChannel = -1;
int32_t OSCMapping::lastHarmonicChordCount = -1;
int32_t OSCMapping::lastHarmonicOctave = -100;

void OSCMapping::sendOSCIfChanged(const char* path, int32_t newValue, int32_t& lastValue) {
	if (lastValue == newValue || !oscController) {
		return;
	}
	lastValue = newValue;
	
	OSCMessage msg(path);
	msg.add(newValue);
	oscController->broadcastFeedback(msg);
}

void OSCMapping::sendOSCIfChanged(const char* path, uint8_t newValue, uint8_t& lastValue) {
	if (lastValue == newValue || !oscController) {
		return;
	}
	lastValue = newValue;
	
	OSCMessage msg(path);
	msg.add((int32_t)newValue);
	oscController->broadcastFeedback(msg);
}

void OSCMapping::sendOSCIfChanged(const char* path, bool newValue, bool& lastValue) {
	if (lastValue == newValue || !oscController) {
		return;
	}
	lastValue = newValue;
	
	OSCMessage msg(path);
	msg.add((int32_t)(newValue ? 1 : 0));
	oscController->broadcastFeedback(msg);
}

void OSCMapping::sendOSCIfChanged(const char* path, float newValue, float& lastValue) {
	if (lastValue == newValue || !oscController) {
		return;
	}
	lastValue = newValue;
	
	OSCMessage msg(path);
	msg.add(newValue);
	oscController->broadcastFeedback(msg);
}

void OSCMapping::sendAllFeedback(EuclideanSequencer* seq, MidiClock* clock) {
	if (!oscController || !seq) return;
	// OSC mapping preference removed — always broadcast OSC feedback
	bool oscEnabled = true;
	
	// Steps
	int32_t steps = seq->getSteps();
	int32_t lastStepsInt = (int32_t)lastSteps;
	if (steps != lastStepsInt && oscEnabled) {
		lastSteps = (uint8_t)steps;
		OSCMessage msg(PATH_STEPS);
		msg.add(steps);
		oscController->broadcastFeedback(msg);
	}
	
	// Hits
	int32_t hits = seq->getHits();
	int32_t lastHitsInt = (int32_t)lastHits;
	if (hits != lastHitsInt && oscEnabled) {
		lastHits = (uint8_t)hits;
		OSCMessage msg(PATH_HITS);
		msg.add(hits);
		oscController->broadcastFeedback(msg);
	}
	
	// Offset
	int32_t offset = seq->getOffset();
	int32_t lastOffsetInt = (int32_t)lastOffset;
	if (offset != lastOffsetInt && oscEnabled) {
		lastOffset = (uint8_t)offset;
		OSCMessage msg(PATH_OFFSET);
		msg.add(offset);
		oscController->broadcastFeedback(msg);
	}
	
	// Velocity
	int32_t velocity = seq->getVelocity();
	int32_t lastVelocityInt = (int32_t)lastVelocity;
	if (velocity != lastVelocityInt && oscEnabled) {
		lastVelocity = (uint8_t)velocity;
		OSCMessage msg(PATH_VELOCITY);
		msg.add(velocity);
		oscController->broadcastFeedback(msg);
	}
	
	// Channel
	int32_t channel = seq->getMidiChannel();
	int32_t lastChannelInt = (int32_t)lastChannel;
	if (channel != lastChannelInt && oscEnabled) {
		lastChannel = (uint8_t)channel;
		OSCMessage msg(PATH_CHANNEL);
		msg.add(channel);
		oscController->broadcastFeedback(msg);
	}
	
	// Resolution
	int32_t res = seq->getResolution();
	int32_t lastResolutionInt = (int32_t)lastResolution;
	if (res != lastResolutionInt && oscEnabled) {
		lastResolution = (uint8_t)res;
		OSCMessage msg(PATH_RESOLUTION);
		msg.add(res);
		oscController->broadcastFeedback(msg);
	}
	
	// Track (quando muda track, atualiza lastNote para evitar enviar feedback de note)
	uint8_t track = seq->getSelectedPattern();
	if (track != lastTrack && oscEnabled) {
		// Quando track muda: enviar both TRACK e NOTE para atualizar clientes OSC
		uint8_t currentNote = seq->getNote();
		lastNote = currentNote; // atualizar cache
		lastTrack = track;
		// Envia TRACK
		OSCMessage msgTrack(PATH_TRACK);
		msgTrack.add((int32_t)track);
		oscController->broadcastFeedback(msgTrack);
		// Envia também NOTE correspondente à track
		OSCMessage msgNote(PATH_NOTE);
		msgNote.add((int32_t)currentNote);
		oscController->broadcastFeedback(msgNote);
		// Não entra no else que envia note condicionalmente
	} else {
		// Note (só envia se mudou E não foi por mudança de track)
		uint8_t currentNote = seq->getNote();
		if (currentNote != lastNote) {
			lastNote = currentNote;
			OSCMessage msg(PATH_NOTE);
			msg.add((int32_t)currentNote);
			oscController->broadcastFeedback(msg);
		}
	}
	
	// Play state (envia apenas a mensagem combinada `/sequencer/playstop` quando muda)
	bool isRunning = clock ? clock->isRunningState() : seq->isPatternRunning();
	bool playChanged = (lastPlayState != isRunning);
	if (playChanged && oscController && oscEnabled) {
		lastPlayState = isRunning;
		OSCMessage msg(PATH_PLAYSTOP);
		// Primeiro parâmetro: start (1 = playing, 0 = not)
		// Segundo parâmetro: stop  (1 = stopped, 0 = not)
		msg.add(isRunning ? 1 : 0);
		msg.add(isRunning ? 0 : 1);
		oscController->broadcastFeedback(msg);
	}
	
	// Tempo
	if (clock) {
		float tempo;
		if (clock->isSlave()) {
			tempo = -1.0f;  // Valor especial para SLAVE
		} else {
			tempo = clock->getBPM();
		}
		if (oscEnabled) sendOSCIfChanged(PATH_TEMPO, tempo, lastTempo);
	}
	
	// Note Length
	uint16_t noteLength = seq->getNoteLength();
	if (noteLength != lastNoteLength && oscEnabled) {
		lastNoteLength = noteLength;
		OSCMessage msg(PATH_NOTE_LENGTH);
		msg.add((int32_t)noteLength);
		oscController->broadcastFeedback(msg);
	}
	
	// Dub state: send per-track path only when state changed
	uint8_t trackIdx = seq->getSelectedPattern();
	bool dubEnabled = seq->isTrackEnabled(trackIdx);
	if (oscEnabled) {
		// Envia apenas quando houver mudança de estado por track (evita envio contínuo)
		for (uint8_t i = 0; i < 8; ++i) {
			bool dubEnabledTrack = seq->isTrackEnabled(i);
			if (dubEnabledTrack != lastDubState[i]) {
				lastDubState[i] = dubEnabledTrack;
				char buf[32];
				snprintf(buf, sizeof(buf), "%s%u", PATH_DUB_BASE, (unsigned)i);
				OSCMessage msgDub(buf);
				msgDub.add((int32_t)(dubEnabledTrack ? 1 : 0));
				oscController->broadcastFeedback(msgDub);
			}
		}
	}
}

void OSCMapping::sendAllFeedbackForced(EuclideanSequencer* seq, MidiClock* clock) {
	if (!oscController || !seq) return;
	bool oscEnabled = true;
	// Steps
	int32_t steps = seq->getSteps();
	lastSteps = (uint8_t)steps;
	OSCMessage msgSteps(PATH_STEPS); msgSteps.add(steps); if (oscEnabled) oscController->broadcastFeedback(msgSteps);

	// Hits
	int32_t hits = seq->getHits(); lastHits = (uint8_t)hits; OSCMessage msgHits(PATH_HITS); msgHits.add(hits); if (oscEnabled) oscController->broadcastFeedback(msgHits);

	// Offset (user-facing)
	int32_t offset = seq->getOffset(); lastOffset = (uint8_t)offset; OSCMessage msgOffset(PATH_OFFSET); msgOffset.add(offset); if (oscEnabled) oscController->broadcastFeedback(msgOffset);

	// Velocity
	int32_t velocity = seq->getVelocity(); lastVelocity = (uint8_t)velocity; OSCMessage msgVel(PATH_VELOCITY); msgVel.add(velocity); if (oscEnabled) oscController->broadcastFeedback(msgVel);

	// Channel
	int32_t channel = seq->getMidiChannel(); lastChannel = (uint8_t)channel; OSCMessage msgChannel(PATH_CHANNEL); msgChannel.add(channel); if (oscEnabled) oscController->broadcastFeedback(msgChannel);

	// Resolution
	int32_t res = seq->getResolution(); lastResolution = (uint8_t)res; OSCMessage msgRes(PATH_RESOLUTION); msgRes.add(res); if (oscEnabled) oscController->broadcastFeedback(msgRes);

	// Track
	uint8_t track = seq->getSelectedPattern(); lastTrack = track; OSCMessage msgTrack(PATH_TRACK); msgTrack.add((int32_t)track); if (oscEnabled) oscController->broadcastFeedback(msgTrack);

	// Note
	uint8_t note = seq->getNote(); lastNote = note; OSCMessage msgNote(PATH_NOTE); msgNote.add((int32_t)note); if (oscEnabled) oscController->broadcastFeedback(msgNote);

	// Play state
	bool isRunning = clock ? clock->isRunningState() : seq->isPatternRunning(); lastPlayState = isRunning; OSCMessage msgPlay(PATH_PLAYSTOP); msgPlay.add(isRunning ? 1 : 0); msgPlay.add(isRunning ? 0 : 1); if (oscEnabled) oscController->broadcastFeedback(msgPlay);

	// Tempo
	if (clock) {
		float tempo = clock->isSlave() ? -1.0f : clock->getBPM(); lastTempo = tempo; OSCMessage msgTempo(PATH_TEMPO); msgTempo.add(tempo); if (oscEnabled) oscController->broadcastFeedback(msgTempo);
	}

	// Note length
	uint16_t noteLength = seq->getNoteLength(); lastNoteLength = noteLength; OSCMessage msgNoteLen(PATH_NOTE_LENGTH); msgNoteLen.add((int32_t)noteLength); if (oscEnabled) oscController->broadcastFeedback(msgNoteLen);

	// Dub (per-track) - send snapshot for all tracks
	for (uint8_t i = 0; i < 8; ++i) {
		bool dubEnabledTrack = seq->isTrackEnabled(i);
		lastDubState[i] = dubEnabledTrack;
		if (oscEnabled) {
			char buf[32];
			snprintf(buf, sizeof(buf), "%s%u", PATH_DUB_BASE, (unsigned)i);
			OSCMessage msgDub(buf);
			msgDub.add((int32_t)(dubEnabledTrack ? 1 : 0));
			oscController->broadcastFeedback(msgDub);
			// Pequeno espaçamento para não saturar o receptor (ex.: Protokol)
			delay(3);
			yield();
		}
	}
}

// Harmonic sequencer OSC feedback (change-based)
void OSCMapping::sendAllHarmonicFeedback(class EuclideanHarmonicSequencer* hseq, MidiClock* clock) {
	if (!oscController || !hseq) return;
	bool oscEnabled = true;

	// Tonality
	int32_t ton = (int32_t)hseq->getTonic();
	sendOSCIfChanged(PATH_HARMONIC_TONALITY, ton, lastHarmonicTonality);

	// Scale
	int32_t scale = (int32_t)hseq->getScaleType();
	if (scale != lastHarmonicScale && oscEnabled) {
		lastHarmonicScale = scale;
		const char* name = OSCMapping::scaleTypeToName(scale);
		// Send combined message: [index, "name"] so clients can read either
		OSCMessage msg(PATH_HARMONIC_SCALE);
		msg.add((int32_t)scale);
		msg.add(name);
		oscController->broadcastFeedback(msg);
		// Also send a separate string-only path for clients that only read
		OSCMessage msgName(PATH_HARMONIC_SCALE_NAME);
		msgName.add(name);
		oscController->broadcastFeedback(msgName);
	}

	// Mode
	int32_t mode = (int32_t)hseq->getDistributionMode();
	sendOSCIfChanged(PATH_HARMONIC_MODE, mode, lastHarmonicMode);

	// Steps
	int32_t steps = (int32_t)hseq->getSteps();
	sendOSCIfChanged(PATH_HARMONIC_STEPS, steps, lastHarmonicSteps);

	// Hits
	int32_t hits = (int32_t)hseq->getHits();
	sendOSCIfChanged(PATH_HARMONIC_HITS, hits, lastHarmonicHits);

	// Offset
	int32_t off = (int32_t)hseq->getOffset();
	sendOSCIfChanged(PATH_HARMONIC_OFFSET, off, lastHarmonicOffset);

	// Poly
	int32_t poly = (int32_t)hseq->getPolyphony();
	sendOSCIfChanged(PATH_HARMONIC_POLY, poly, lastHarmonicPoly);

	// Velocity
	int32_t vel = (int32_t)hseq->getVelocity();
	sendOSCIfChanged(PATH_HARMONIC_VELOCITY, vel, lastHarmonicVelocity);

	// Octave (base octave -2..+2)
	int32_t oct = (int32_t)hseq->getBaseOctave();
	sendOSCIfChanged(PATH_HARMONIC_OCTAVE, oct, lastHarmonicOctave);

	// Note length
	int32_t nlen = (int32_t)hseq->getNoteLength();
	sendOSCIfChanged(PATH_HARMONIC_NOTE_LENGTH, nlen, lastHarmonicNoteLength);

	// Active
	int32_t active = hseq->isActive() ? 1 : 0;
	sendOSCIfChanged(PATH_HARMONIC_ACTIVE, active, lastHarmonicActive);

	// Per-track Active (independente por track): /harmonic/active/<n>
	for (uint8_t i = 0; i < EuclideanHarmonicSequencer::MAX_TRACKS; ++i) {
		int32_t trackActive = hseq->isTrackActive(i) ? 1 : 0;
		if (lastHarmonicTrackActive[i] != trackActive) {
			lastHarmonicTrackActive[i] = trackActive;
			char buf[32];
			snprintf(buf, sizeof(buf), "%s/%u", PATH_HARMONIC_ACTIVE, (unsigned)(i + 1));
			OSCMessage msg(buf);
			msg.add(trackActive);
			oscController->broadcastFeedback(msg);
		}
	}

	// Track (active track number 1..N)
	int32_t track = (int32_t)hseq->getActiveTrackNumber();
	sendOSCIfChanged(PATH_HARMONIC_TRACK, track, lastHarmonicTrack);

	// Resolution (index)
	int32_t res = (int32_t)hseq->getResolutionIndex();
	sendOSCIfChanged(PATH_HARMONIC_RESOLUTION, res, lastHarmonicResolution);

	// MIDI Channel
	int32_t ch = (int32_t)hseq->getMidiChannel();
	sendOSCIfChanged(PATH_HARMONIC_CHANNEL, ch, lastHarmonicChannel);

	// Chord count
	int32_t chordCount = (int32_t)hseq->getChordListSize();
	sendOSCIfChanged(PATH_HARMONIC_CHORDS_COUNT, chordCount, lastHarmonicChordCount);

	// Chord slots (per-slot send only when state changed)
	for (uint8_t i = 0; i < 8; ++i) {
		int32_t exists = (i < hseq->getChordListSize()) ? 1 : 0;
		if (lastHarmonicChordSlot[i] != exists) {
			lastHarmonicChordSlot[i] = exists;
			char buf[32];
			snprintf(buf, sizeof(buf), "%s/chord/%u", PATH_HARMONIC_BASE, (unsigned)i);
			OSCMessage msg(buf);
			msg.add((int32_t)exists);
			oscController->broadcastFeedback(msg);
		}
	}
}

// Harmonic forced snapshot: broadcast all parameters (ignore caches), then update caches
void OSCMapping::sendAllHarmonicFeedbackForced(class EuclideanHarmonicSequencer* hseq, MidiClock* clock) {
	if (!oscController || !hseq) return;
	bool oscEnabled = true;

	// Tonality
	{
		OSCMessage msg(PATH_HARMONIC_TONALITY);
		msg.add((int32_t)hseq->getTonic());
		oscController->broadcastFeedback(msg);
	}

	// Scale
	{
		const char* name = OSCMapping::scaleTypeToName((int)hseq->getScaleType());
		int32_t idx = (int32_t)hseq->getScaleType();
		// Combined message: index + name
		OSCMessage msg(PATH_HARMONIC_SCALE);
		msg.add((int32_t)idx);
		msg.add(name);
		oscController->broadcastFeedback(msg);
		// Separate name-only path for clients like TouchOSC
		OSCMessage msgName(PATH_HARMONIC_SCALE_NAME);
		msgName.add(name);
		oscController->broadcastFeedback(msgName);
	}

	// Mode
	{
		OSCMessage msg(PATH_HARMONIC_MODE);
		msg.add((int32_t)hseq->getDistributionMode());
		oscController->broadcastFeedback(msg);
	}

	// Steps
	{
		OSCMessage msg(PATH_HARMONIC_STEPS);
		msg.add((int32_t)hseq->getSteps());
		oscController->broadcastFeedback(msg);
	}

	// Hits
	{
		OSCMessage msg(PATH_HARMONIC_HITS);
		msg.add((int32_t)hseq->getHits());
		oscController->broadcastFeedback(msg);
	}

	// Offset
	{
		OSCMessage msg(PATH_HARMONIC_OFFSET);
		msg.add((int32_t)hseq->getOffset());
		oscController->broadcastFeedback(msg);
	}

	// Polyphony
	{
		OSCMessage msg(PATH_HARMONIC_POLY);
		msg.add((int32_t)hseq->getPolyphony());
		oscController->broadcastFeedback(msg);
	}

	// Velocity
	{
		OSCMessage msg(PATH_HARMONIC_VELOCITY);
		msg.add((int32_t)hseq->getVelocity());
		oscController->broadcastFeedback(msg);
	}

	// Note length
	{
		OSCMessage msg(PATH_HARMONIC_NOTE_LENGTH);
		msg.add((int32_t)hseq->getNoteLength());
		oscController->broadcastFeedback(msg);
	}

	// Octave
	{
		OSCMessage msg(PATH_HARMONIC_OCTAVE);
		msg.add((int32_t)hseq->getBaseOctave());
		oscController->broadcastFeedback(msg);
	}

	// Active
	{
		OSCMessage msg(PATH_HARMONIC_ACTIVE);
		msg.add((int32_t)(hseq->isActive() ? 1 : 0));
		oscController->broadcastFeedback(msg);
	}

	// Per-track Active snapshot: /harmonic/active/<n>
	for (uint8_t i = 0; i < EuclideanHarmonicSequencer::MAX_TRACKS; ++i) {
		char buf[32];
		int32_t trackActive = hseq->isTrackActive(i) ? 1 : 0;
		lastHarmonicTrackActive[i] = trackActive;
		snprintf(buf, sizeof(buf), "%s/%u", PATH_HARMONIC_ACTIVE, (unsigned)(i + 1));
		OSCMessage msg(buf);
		msg.add(trackActive);
		oscController->broadcastFeedback(msg);
	}

	// Track
	{
		OSCMessage msg(PATH_HARMONIC_TRACK);
		msg.add((int32_t)hseq->getActiveTrackNumber());
		oscController->broadcastFeedback(msg);
	}

	// Resolution
	{
		OSCMessage msg(PATH_HARMONIC_RESOLUTION);
		msg.add((int32_t)hseq->getResolutionIndex());
		oscController->broadcastFeedback(msg);
	}

	// MIDI Channel
	{
		OSCMessage msg(PATH_HARMONIC_CHANNEL);
		msg.add((int32_t)hseq->getMidiChannel());
		oscController->broadcastFeedback(msg);
	}

	// Chord count
	{
		OSCMessage msg(PATH_HARMONIC_CHORDS_COUNT);
		msg.add((int32_t)hseq->getChordListSize());
		oscController->broadcastFeedback(msg);
	}

	// Chord slots
	for (uint8_t i = 0; i < 8; ++i) {
		char buf[32];
		snprintf(buf, sizeof(buf), "%s/chord/%u", PATH_HARMONIC_BASE, (unsigned)i);
		OSCMessage msg(buf);
		msg.add((int32_t)((i < hseq->getChordListSize()) ? 1 : 0));
		oscController->broadcastFeedback(msg);
	}

	// Finally update caches so subsequent change-based sends work
	lastHarmonicTonality = (int32_t)hseq->getTonic();
	lastHarmonicScale = (int32_t)hseq->getScaleType();
	lastHarmonicMode = (int32_t)hseq->getDistributionMode();
	lastHarmonicSteps = (int32_t)hseq->getSteps();
	lastHarmonicHits = (int32_t)hseq->getHits();
	lastHarmonicOffset = (int32_t)hseq->getOffset();
	lastHarmonicPoly = (int32_t)hseq->getPolyphony();
	lastHarmonicVelocity = (int32_t)hseq->getVelocity();
	lastHarmonicOctave = (int32_t)hseq->getBaseOctave();
	lastHarmonicNoteLength = (int32_t)hseq->getNoteLength();
	lastHarmonicActive = hseq->isActive() ? 1 : 0;
	for (uint8_t i = 0; i < 8; ++i) {
		lastHarmonicChordSlot[i] = (i < hseq->getChordListSize()) ? 1 : 0;
		lastHarmonicTrackActive[i] = hseq->isTrackActive(i) ? 1 : 0;
	}

	// Also update additional caches
	lastHarmonicTrack = (int32_t)hseq->getActiveTrackNumber();
	lastHarmonicResolution = (int32_t)hseq->getResolutionIndex();
	lastHarmonicChannel = (int32_t)hseq->getMidiChannel();
	lastHarmonicChordCount = (int32_t)hseq->getChordListSize();
}

const char* OSCMapping::scaleTypeToName(int scaleType) {
	static const char* names[] = {
		"Major",
		"Natural Minor",
		"Harmonic Minor",
		"Melodic Minor",
		"Ionian",
		"Dorian",
		"Phrygian",
		"Lydian",
		"Mixolydian",
		"Aeolian",
		"Locrian",
		"Mix b9 b13",
		"Lydian #9",
		"Phrygian Dominant",
		"Lydian #5",
		"Half-Whole",
		"Whole-Half",
		"Augmented",
		"Altered",
		"Pent Major",
		"Pent Minor",
		"Blues",
		"Gypsy",
		"Japanese",
		"Hirajoshi",
		"Kumoi",
		"Arabic"
	};
	if (scaleType < 0) return "Unknown";
	if (scaleType >= (int)EuclideanHarmonicSequencer::SCALE_COUNT) return "Unknown";
	return names[scaleType];
}

