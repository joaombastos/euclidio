#ifndef MIDI_FEEDBACK_H
#define MIDI_FEEDBACK_H

#include <stdint.h>
#include <Adafruit_TinyUSB.h>
#include "Control_Surface.h"

class EuclideanSequencer;
class MidiClock;

// Classe para enviar feedback MIDI CC dos estados atuais do sequenciador
// Usa change-based detection para minimizar tráfego MIDI
class MidiFeedback {
	public:
	// Inicializa o modulo com referências aos interfaces MIDI
	static void begin(Adafruit_USBD_MIDI* usb = nullptr);
	static void setBluetoothInterface(cs::BluetoothMIDI_Interface* ble = nullptr);	// Verifica e envia feedbacks apenas se valores mudaram (chamado periodicamente)
	static void setEuclideanSequencer(EuclideanSequencer* seq);
	static void sendAllFeedback(EuclideanSequencer* seq, MidiClock* clock);
	// Envia todos os feedbacks ignorando cache (útil para snapshot on-entry)
	static void sendAllFeedbackForced(EuclideanSequencer* seq, MidiClock* clock);
	// Forçar envio ignorando state cache: útil para enviar snapshot completo
	static void resetFeedbackState();
	// Envia uma nota como feedback (Note On/Off) para USB/BLE/DIN
	static void sendNote(uint8_t note, uint8_t velocity, uint8_t channel = 9);
	
	// Controle de quais feedbacks enviar
	static void setFeedbackEnabled(bool enabled) { feedbackEnabled = enabled; }
	static bool isFeedbackEnabled() { return feedbackEnabled; }
	
	private:
	static Adafruit_USBD_MIDI* usb_midi;
	static cs::BluetoothMIDI_Interface* ble_midi;
	static bool feedbackEnabled;	// Valores anteriores para detecção de mudanças
	static EuclideanSequencer* eucl_seq;
	static uint8_t lastStepsCC;
	static uint8_t lastHitsCC;
	static uint8_t lastOffsetCC;
	static uint8_t lastNoteCC;
	static uint8_t lastVelocityCC;
	static uint8_t lastChannelCC;
	static uint8_t lastResolutionCC;
	static uint8_t lastTrackCC;
	static uint8_t lastPlayCC;
	static uint8_t lastTempoCC;
	static uint8_t lastNoteLengthCC;
	// Cache per-track para estado de DUB (sentinela -1 = desconhecido)
	static int8_t lastDubState[8];
	
	static void sendCCIfChanged(uint8_t cc, uint8_t newValue, uint8_t& lastValue, uint8_t channel = 9);
};

#endif // MIDI_FEEDBACK_H
