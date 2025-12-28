#include "MidiCCMapping.h"
#include "EuclideanSequencer.h"
#include "MidiClock.h"
#include "MidiFeedback.h"
// Harmonic sequencer access
#include "EuclideanHarmonicSequencer.h"
// App state (appMode, harmonic edit flags)
#include "AppState.h"
// OSC feedback mappings
#include "OSCMapping.h"

// harmonicSeq e índice de edição de chord list (definidos em main.cpp)
extern EuclideanHarmonicSequencer harmonicSeq;
extern uint8_t harmonicChordEditIndex;

// acesso ao MidiClock global (declarado em main.cpp)
extern MidiClock midiClock;

// Forward declarations para callbacks MIDI encoder (definidos em main.cpp)
extern "C" {
	void midiEncoderRotation(int8_t direction);
	void midiEncoderDoubleClick();
	void midiEncoderLongPress();
}

void MidiCCMapping::processCC(uint8_t channel, uint8_t cc, uint8_t value, EuclideanSequencer* seq, MidiClock* clock) {
	if (!seq || channel != MIDI_CONTROL_CHANNEL) return;
	
	switch (cc) {
		case CC_TRACK:
			seq->setSelectedPattern(mapCCToTrack(value));
			break;
		case CC_CHANNEL:
			// Ajusta o canal MIDI do sequenciador (0-15)
			seq->setMidiChannel(mapCCToChannel(value));
			// Enviar feedback imediato do canal
			MidiFeedback::sendAllFeedback(seq, clock);
			break;
		case CC_RESOLUTION:
			seq->setResolution(mapCCToResolution(value));
			break;
		case CC_TEMPO:
			// Tempo é controlado via MidiClock: mapear 0-127 -> 30-240 BPM
			{
				int bpm = mapCCToTempo(value);
				if (clock) {
					clock->setBPM((float)bpm);
					// Entrar em SLAVE somente se value >= 64; caso contrário usar MASTER
					if (value >= 127) {
						clock->setSyncMode(MidiClock::SLAVE);
					} else {
						clock->setSyncMode(MidiClock::MASTER);
					}
				} else {
					// usa instância global caso não receba ponteiro
					midiClock.setBPM((float)bpm);
					if (value >= 127) {
						midiClock.setSyncMode(MidiClock::SLAVE);
					} else {
						midiClock.setSyncMode(MidiClock::MASTER);
					}
				}
				break;
			}
		case CC_NOTE_LENGTH:
			// Note Length: mapear 0-127 para 50-700ms
			seq->setNoteLength(mapCCToNoteLength(value));
			// Enviar feedback do note length
			MidiFeedback::sendAllFeedback(seq, clock);
			break;

		// Harmonic sequencer CCs (40..57)
		case CC_HARM_TONIC:
			harmonicSeq.setTonic(mapCCToTonic(value));
			break;
		case CC_HARM_SCALE:
			{
				int idx = mapCCToScaleIndex(value);
				if (idx >= 0 && idx < (int)EuclideanHarmonicSequencer::SCALE_COUNT) {
					harmonicSeq.setScaleType((EuclideanHarmonicSequencer::ScaleType)idx);
				}
			}
			break;
		case CC_HARM_MODE:
			harmonicSeq.setDistributionMode(value < 64 ? EuclideanHarmonicSequencer::DIST_CHORDS : EuclideanHarmonicSequencer::DIST_NOTES);
			break;
		case CC_HARM_STEPS:
			harmonicSeq.setSteps(mapCCToSteps(value));
			break;
		case CC_HARM_HITS:
			harmonicSeq.setHits(mapCCToHits(value));
			break;
		case CC_HARM_OFFSET:
			harmonicSeq.setOffset(mapCCToOffset(value));
			break;
		case CC_HARM_POLY:
			harmonicSeq.setPolyphony(mapCCToPolyphony(value));
			break;
		case CC_HARM_VELOCITY:
			harmonicSeq.setVelocity(mapCCToVelocity(value));
			break;
		case CC_HARM_NOTE_LENGTH:
			harmonicSeq.setNoteLength(mapCCToLargeNoteLength(value));
			break;
		case CC_HARM_OCTAVE:
			// Ajuste de oitava para o sequenciador harmônico (-2..+2)
			harmonicSeq.setBaseOctave((int8_t)constrain(map(value, 0, 127, -2, 2), -2, 2));
			break;
		case CC_HARM_RESOLUTION: {
			// Mapear 0..127 -> resolution index 0..2 (0=1/4,1=1/8,2=1/16)
			int idx = constrain(map(value, 0, 127, 0, 2), 0, 2);
			harmonicSeq.setResolutionIndex((uint8_t)idx);
			break;
		}
		case CC_HARM_CHORD_SELECT:
			harmonicChordEditIndex = map(value, 0, 127, 0, 15); // cap at reasonable max (0..15)
			break;
		case CC_HARM_CHORD_SET_DEGREE:
			{
				uint8_t deg = mapCCToDegree(value);
				// set at selected index if valid
				harmonicSeq.setChordListItem(harmonicChordEditIndex, deg);
			}
			break;
		case CC_HARM_CHORD_INSERT:
			{
				uint8_t deg = mapCCToDegree(value);
				harmonicSeq.insertChordAt(harmonicChordEditIndex, deg);
			}
			break;
		case CC_HARM_CHORD_DELETE:
			harmonicSeq.removeChordAt(harmonicChordEditIndex);
			break;
		// Group2 handling removed (deprecated CCs 54..56)
		// CC_ENCODER_ROTATION removed: encoder rotation is no longer mapped to CC messages
		/* CC_PRESET removed: preset load/save via CC 30 disabled */
	}
}

void MidiCCMapping::processNote(uint8_t channel, uint8_t note, uint8_t velocity, bool isNoteOn, EuclideanSequencer* seq) {
	if (channel != MIDI_CONTROL_CHANNEL || !isNoteOn || velocity == 0) return;
	
	// Encoder long press (Note 50)
	if (note == NOTE_ENCODER_LONG_PRESS) {
		midiEncoderLongPress();
		return;
	}

	// Encoder double-click handled via NOTE_ENCODER_DOUBLE_CLICK
	if (note == NOTE_ENCODER_DOUBLE_CLICK) {
		midiEncoderDoubleClick();
		return;
	}

	// Play/Stop universal via Note On 0
	if (note == NOTE_TOGGLE_PLAY_ALT) {
		if (midiClock.isRunningState()) {
			midiClock.stop();
			seq->stop();
			seq->setPlayMode(EuclideanSequencer::STOP);
		} else {
			midiClock.start();
			seq->start();
			seq->setPlayMode(EuclideanSequencer::PLAY);
		}
		// feedback: enviar nota 0 (127 = playing, 0 = stopped)
		MidiFeedback::sendNote(NOTE_TOGGLE_PLAY_ALT, midiClock.isRunningState() ? 127 : 0);
		return;
	}

	// Harmonic sequencer note controls
	if (note == NOTE_HARMONIC_TOGGLE) {
		// Toggle harmonic sequencer active state and switch UI mode
		bool newState = !harmonicSeq.isActive();
		harmonicSeq.setActive(newState);
		if (newState) {
			lastAppMode = appMode;
			appMode = MODE_HARMONIC;
		} else {
			appMode = lastAppMode;
		}
		// Send forced feedback to update controllers
		MidiFeedback::sendAllHarmonicFeedbackForced(&harmonicSeq, &midiClock);
		OSCMapping::sendAllHarmonicFeedbackForced(&harmonicSeq, &midiClock);
		return;
	}

	if (note == NOTE_HARMONIC_CHORD_EDIT) {
		// Toggle chord edit mode (used by UI/encoders)
		harmonicChordEditMode = !harmonicChordEditMode;
		return;
	}

	// Toggle chord slot enable/disable (notes 92..99 -> slots 0..7)
	if (note >= NOTE_HARMONIC_SLOT_BASE && note < NOTE_HARMONIC_SLOT_BASE + 8) {
		uint8_t slot = note - NOTE_HARMONIC_SLOT_BASE;
		if (slot < harmonicSeq.getChordListSize()) {
			// If slot exists, remove it
			harmonicSeq.removeChordAt(slot);
		} else {
			// Otherwise insert a default degree (0)
			harmonicSeq.insertChordAt(slot, 0);
		}
		MidiFeedback::sendAllHarmonicFeedbackForced(&harmonicSeq, &midiClock);
		OSCMapping::sendAllHarmonicFeedbackForced(&harmonicSeq, &midiClock);
		return;
	}
	
	// Routing matrix notes (notas 51-65: 5 entradas × 3 saídas DIN)
	// Routing matrix MIDI note mapping removed — notas 51..65 não são mais usadas para controlar a matriz

	// Toggle Dub por track (notas NOTE_TOGGLE_DUB_BASE .. NOTE_TOGGLE_DUB_BASE+7)
	if (seq) {
		if (note >= MidiCCMapping::NOTE_TOGGLE_DUB_BASE && note < MidiCCMapping::NOTE_TOGGLE_DUB_BASE + 8) {
			uint8_t trackIdx = note - MidiCCMapping::NOTE_TOGGLE_DUB_BASE;
			bool enabled = seq->isTrackEnabled(trackIdx);
			seq->setTrackEnabled(trackIdx, !enabled);
			// feedback: enviar nota correspondente à track
			MidiFeedback::sendNote(MidiCCMapping::NOTE_TOGGLE_DUB_BASE + trackIdx, (!enabled) ? 127 : 0);
			return;
		}
	}


	// Group toggle/step via NOTE 78 (F#5) - velocity >=64 => next, <64 => previous
	if (note == NOTE_TOGGLE_GROUP) {
		if (velocity >= 64) {
			seq->nextGroup();
		} else {
			seq->prevGroup();
		}
		return;
	}
}

// Funções de conversão de CC (0-127) para ranges apropriados
int MidiCCMapping::mapCCToSteps(uint8_t value) {
	// 0-127 → 1-32
	return constrain(map(value, 0, 127, 1, 32), 1, 32);
}

int MidiCCMapping::mapCCToHits(uint8_t value) {
	// 0-127 → 1-32
	return constrain(map(value, 0, 127, 1, 32), 1, 32);
}

int MidiCCMapping::mapCCToOffset(uint8_t value) {
	// 0-127 → 0-31
	return map(value, 0, 127, 0, 31);
}

int MidiCCMapping::mapCCToNote(uint8_t value) {
	// 0-127 → 0-127 (1:1)
	return value;
}

int MidiCCMapping::mapCCToVelocity(uint8_t value) {
	// 0-127 → 0-127 (1:1)
	return value;
}

int MidiCCMapping::mapCCToChannel(uint8_t value) {
	// 0-127 → 0-15 (16 canais MIDI)
	return map(value, 0, 127, 0, 15);
}

int MidiCCMapping::mapCCToResolution(uint8_t value) {
	// 0-127 → 1-4
	// 0-31: res 1, 32-63: res 2, 64-95: res 3, 96-127: res 4
	return constrain(map(value, 0, 127, 1, 4), 1, 4);
}

int MidiCCMapping::mapCCToTrack(uint8_t value) {
	// 0-127 → 0-7 (8 tracks)
	return map(value, 0, 127, 0, 7);
}

int MidiCCMapping::mapCCToTempo(uint8_t value) {
	// 0-127 → 30-240 BPM
	return constrain(map(value, 0, 127, 30, 240), 30, 240);
}

int MidiCCMapping::mapCCToNoteLength(uint8_t value) {
	// 0-127 → 50-700ms
	return constrain(map(value, 0, 127, 50, 700), 50, 700);
}

uint8_t MidiCCMapping::mapCCToTonic(uint8_t value) {
	return (uint8_t)map(value, 0, 127, 0, 11);
}

int MidiCCMapping::mapCCToScaleIndex(uint8_t value) {
	int maxIdx = (int)EuclideanHarmonicSequencer::SCALE_COUNT - 1;
	return constrain(map(value, 0, 127, 0, maxIdx), 0, maxIdx);
}

uint8_t MidiCCMapping::mapCCToPolyphony(uint8_t value) {
	return (uint8_t)constrain(map(value, 0, 127, 1, 8), 1, 8);
}

int MidiCCMapping::mapCCToLargeNoteLength(uint8_t value) {
	// 0-127 -> 10-2000 ms
	return constrain(map(value, 0, 127, 10, 2000), 10, 2000);
}

uint8_t MidiCCMapping::mapCCToDegree(uint8_t value) {
	// Map 0-127 to 0-6 (scale degrees)
	return (uint8_t)map(value, 0, 127, 0, 6);
}

// ===== FEEDBACK MIDI CC =====
// Envia o estado atual dos parâmetros via MIDI CC

void MidiCCMapping::sendFeedback(EuclideanSequencer* seq, MidiClock* clock) {
	if (!seq) return;
	
	// Envia todos os estados atuais
	sendFeedbackSteps(seq);
	sendFeedbackHits(seq);
	sendFeedbackOffset(seq);
	sendFeedbackNote(seq);
	sendFeedbackVelocity(seq);
	sendFeedbackChannel(seq);
	sendFeedbackResolution(seq);
	sendFeedbackTrack(seq);
	sendFeedbackPlay(seq, clock);
	if (clock) {
		sendFeedbackTempo(clock);
	}
}

void MidiCCMapping::sendFeedbackSteps(EuclideanSequencer* seq) {
	if (!seq) return;
	uint8_t steps = seq->getSteps();
	// Mapear 1-32 → 0-127
	uint8_t ccValue = constrain(map(steps, 1, 32, 0, 127), 0, 127);
	// Enviar via MIDI (será implementado via EuclideanMidiEngine)
	// Por enquanto, apenas registramos a função
}

void MidiCCMapping::sendFeedbackHits(EuclideanSequencer* seq) {
	if (!seq) return;
	uint8_t hits = seq->getHits();
	// Mapear 1-32 → 0-127
	uint8_t ccValue = constrain(map(hits, 1, 32, 0, 127), 0, 127);
}

void MidiCCMapping::sendFeedbackOffset(EuclideanSequencer* seq) {
	if (!seq) return;
	uint8_t offset = seq->getOffset();
	// Mapear 0-31 → 0-127
	uint8_t ccValue = map(offset, 0, 31, 0, 127);
}

void MidiCCMapping::sendFeedbackNote(EuclideanSequencer* seq) {
	if (!seq) return;
	uint8_t note = seq->getNote();
	// Mapear 0-127 → 0-127 (1:1)
	uint8_t ccValue = note;
}

void MidiCCMapping::sendFeedbackVelocity(EuclideanSequencer* seq) {
	if (!seq) return;
	uint8_t velocity = seq->getVelocity();
	// Mapear 0-127 → 0-127 (1:1)
	uint8_t ccValue = velocity;
}

void MidiCCMapping::sendFeedbackResolution(EuclideanSequencer* seq) {
	if (!seq) return;
	uint8_t res = seq->getResolution();
	// Mapear 1-4 → 0-127
	uint8_t ccValue = map(res, 1, 4, 0, 127);
}

void MidiCCMapping::sendFeedbackTrack(EuclideanSequencer* seq) {
	if (!seq) return;
	uint8_t track = seq->getSelectedPattern();
	// Mapear 0-7 → 0-127
	uint8_t ccValue = map(track, 0, 7, 0, 127);
}

void MidiCCMapping::sendFeedbackPlay(EuclideanSequencer* seq, MidiClock* clock) {
	if (!seq) return;
	bool isRunning = clock ? clock->isRunningState() : seq->isPatternRunning();
	// Enviar 0 (parado) ou 127 (tocando)
	uint8_t ccValue = isRunning ? 127 : 0;
}

void MidiCCMapping::sendFeedbackTempo(MidiClock* clock) {
	if (!clock) return;
	
	// Diferentes modos de feedback para tempo
	if (clock->isSlave()) {
		// Em modo SLAVE, enviar valor especial
		uint8_t ccValue = 127;  // Ou algum valor que indique Slave
	} else {
		// Em modo MASTER, enviar BPM mapeado para 0-127
		float bpm = clock->getBPM();
		uint8_t ccValue = constrain(map((int)bpm, 30, 240, 0, 127), 0, 127);
	}
}

