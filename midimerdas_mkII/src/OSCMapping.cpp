#include "OSCMapping.h"
#include "EuclideanSequencer.h"
#include "MidiClock.h"
#include "OSCController.h"
#include "RoutingMatrix.h"
#include <Arduino.h>
#include <string.h>
#include <OSCMessage.h>

// Forward declarations para callbacks (definidos em main.cpp)
extern "C" {
	void midiEncoderRotation(int8_t direction);
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
		midiEncoderDoubleClick();
	} else if (strcmp(path, PATH_ENCODER_LONG_PRESS) == 0) {
		midiEncoderLongPress();
	}
	// Novo formato: mensagens independentes por rota: /routing/<source>/<dest>
	// Ex: /routing/in1/out2  ou /routing/usb/out1
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
	// Check OSCMap setting on sequencer: true = enabled, false = suppress
	bool oscEnabled = seq->getOutputOSCMap();
	
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
		// Dub: enviar todos os 8 estados por-track (forced feedback usado no arranque)
		for (uint8_t i = 0; i < 8; ++i) {
			bool dubEnabledTrack = seq->isTrackEnabled(i);
			lastDubState[i] = dubEnabledTrack;
			if (oscEnabled) {
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
	bool oscEnabled = seq->getOutputOSCMap();
	// Steps
	int32_t steps = seq->getSteps();
	lastSteps = (uint8_t)steps;
	OSCMessage msgSteps(PATH_STEPS); msgSteps.add(steps); oscController->broadcastFeedback(msgSteps);

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

	// Dub (per-track) - send only for selected track
	uint8_t trackIdx = seq->getSelectedPattern(); bool dubEnabled = seq->isTrackEnabled(trackIdx);
	lastDubState[trackIdx] = dubEnabled;
	if (oscEnabled) {
		char buf[32];
		snprintf(buf, sizeof(buf), "%s%u", PATH_DUB_BASE, (unsigned)trackIdx);
		OSCMessage msgDub(buf); msgDub.add((int32_t)(dubEnabled ? 1 : 0)); oscController->broadcastFeedback(msgDub);
	}
}

