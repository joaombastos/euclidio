#include "MidiFeedback.h"
#include "EuclideanSequencer.h"
#include "MidiClock.h"
#include "MidiCCMapping.h"
#include "MIDIRouter.h"
#include "Control_Surface.h"

// Local base note for DUB per-track mapping (matches MidiCCMapping note base)
static const uint8_t DUB_NOTE_BASE = 76;

// Instâncias estáticas
Adafruit_USBD_MIDI* MidiFeedback::usb_midi = nullptr;
cs::BluetoothMIDI_Interface* MidiFeedback::ble_midi = nullptr;
bool MidiFeedback::feedbackEnabled = true;
EuclideanSequencer* MidiFeedback::eucl_seq = nullptr;

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

void MidiFeedback::begin(Adafruit_USBD_MIDI* usb) {
	usb_midi = usb;
	feedbackEnabled = true;
}

void MidiFeedback::setEuclideanSequencer(EuclideanSequencer* seq) {
    eucl_seq = seq;
}

// Envia todos os CCs independentemente do cache de valores anteriores
void MidiFeedback::sendAllFeedbackForced(EuclideanSequencer* seq, MidiClock* clock) {
	if (!feedbackEnabled || !seq) return;
	uint8_t channel = 9;
	// Check mapping preference (simple ON/OFF): if false, do not send any MIDI feedback
	bool midiEnabled = seq->getOutputMidiMap();
	auto sendDirect = [&](uint8_t cc, uint8_t value) {
		if (!midiEnabled) return;
		uint8_t statusByte = 0xB0 | (channel & 0x0F);
		// USB
		if (usb_midi) {
			usb_midi->write(statusByte);
			usb_midi->write(cc & 0x7F);
			usb_midi->write(value & 0x7F);
		}
		// BLE
		if (ble_midi) {
			ble_midi->send(cs::ChannelMessage(statusByte, cc & 0x7F, value & 0x7F));
			ble_midi->sendNow();
		}
		// DINs (all) - send to all DIN outputs when enabled
		MIDIRouter::sendToOutput(0, statusByte);
		MIDIRouter::sendToOutput(0, cc & 0x7F);
		MIDIRouter::sendToOutput(0, value & 0x7F);
		MIDIRouter::sendToOutput(1, statusByte);
		MIDIRouter::sendToOutput(1, cc & 0x7F);
		MIDIRouter::sendToOutput(1, value & 0x7F);
		MIDIRouter::sendToOutput(2, statusByte);
		MIDIRouter::sendToOutput(2, cc & 0x7F);
		MIDIRouter::sendToOutput(2, value & 0x7F);
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
}

void MidiFeedback::setBluetoothInterface(cs::BluetoothMIDI_Interface* ble) {
	ble_midi = ble;
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
	bool midiEnabled = seq->getOutputMidiMap();
	
	// Função lambda auxiliar para enviar com o protocolo (respeita midiEnabled)
	auto sendCCWithProtocol = [&](uint8_t cc, uint8_t newValue, uint8_t& lastValue) {
		if (!midiEnabled) return;
		if (lastValue == newValue) return;
		lastValue = newValue;
		
		uint8_t statusByte = 0xB0 | (channel & 0x0F);
		
		if (usb_midi) {
			usb_midi->write(statusByte);
			usb_midi->write(cc & 0x7F);
			usb_midi->write(newValue & 0x7F);
		}
		if (ble_midi) {
			ble_midi->send(cs::ChannelMessage(statusByte, cc & 0x7F, newValue & 0x7F));
			ble_midi->sendNow();
		}
		// DINs
		MIDIRouter::sendToOutput(0, statusByte);  // DIN1
		MIDIRouter::sendToOutput(0, cc & 0x7F);
		MIDIRouter::sendToOutput(0, newValue & 0x7F);
		MIDIRouter::sendToOutput(1, statusByte);  // DIN2
		MIDIRouter::sendToOutput(1, cc & 0x7F);
		MIDIRouter::sendToOutput(1, newValue & 0x7F);
		MIDIRouter::sendToOutput(2, statusByte);  // DIN3
		MIDIRouter::sendToOutput(2, cc & 0x7F);
		MIDIRouter::sendToOutput(2, newValue & 0x7F);
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
	
	// Play state feedback removed: Play/Stop agora é controlado por NOTE 77
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

	// If euclidean sequencer pointer is set, consult MidiMap (simple ON/OFF)
	if (eucl_seq && !eucl_seq->getOutputMidiMap()) return;

	uint8_t status = (velocity > 0) ? (0x90 | (channel & 0x0F)) : (0x80 | (channel & 0x0F));

	if (usb_midi) {
		usb_midi->write(status);
		usb_midi->write(note & 0x7F);
		usb_midi->write(velocity & 0x7F);
	}
	if (ble_midi) {
		ble_midi->send(cs::ChannelMessage(status, note & 0x7F, velocity & 0x7F));
		ble_midi->sendNow();
	}
	// DIN outputs (always send to all DIN ports when enabled)
	MIDIRouter::sendToOutput(0, status);
	MIDIRouter::sendToOutput(0, note & 0x7F);
	MIDIRouter::sendToOutput(0, velocity & 0x7F);
	MIDIRouter::sendToOutput(1, status);
	MIDIRouter::sendToOutput(1, note & 0x7F);
	MIDIRouter::sendToOutput(1, velocity & 0x7F);
	MIDIRouter::sendToOutput(2, status);
	MIDIRouter::sendToOutput(2, note & 0x7F);
	MIDIRouter::sendToOutput(2, velocity & 0x7F);
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

