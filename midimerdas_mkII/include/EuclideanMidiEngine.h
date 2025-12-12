#ifndef EUCLIDEAN_MIDI_ENGINE_H
#define EUCLIDEAN_MIDI_ENGINE_H

#include <Arduino.h>
#include "EuclideanSequencer.h"
#include "MidiClock.h"
// FreeRTOS primitives
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// Forward declarations
namespace cs {
	class BluetoothMIDI_Interface;
}
class Adafruit_USBD_MIDI;
class OSCController;
class MIDIRouter;

class EuclideanMidiEngine {
private:
	EuclideanSequencer* euclSeq = nullptr;
	MidiClock* clock = nullptr;
	cs::BluetoothMIDI_Interface* ble_midi = nullptr;
	Adafruit_USBD_MIDI* usb_midi = nullptr;
	OSCController* osc = nullptr;
	
	unsigned long lastBpmUpdateTime = 0;
	
	// Tabela de ticks por resolução: índice 0-3 para resolution 1-4
	static const uint8_t ticksPerResolution[4];
	
	// Fila unificada para USB MIDI (não-bloqueante)
	static const uint16_t MIDI_QUEUE_SIZE = 512;  // Aumentado para evitar overflow
	struct MidiEvent {
		uint8_t channel;
		uint8_t note;
		uint8_t velocity;
		uint16_t noteLength;  // Duração da nota em ms
		bool isNoteOn;
		// Se precisar diferenciar: podem ser enviados para ambos
	};
	MidiEvent midiQueue[MIDI_QUEUE_SIZE];
	volatile uint16_t midiQueueHead = 0;
	volatile uint16_t midiQueueTail = 0;
	volatile uint32_t midiDroppedEvents = 0;

	// Worker task and semaphore para processar a fila sem bloquear o loop principal
	TaskHandle_t midiWorkerHandle = nullptr;
	SemaphoreHandle_t midiQueueSem = nullptr;
	
	// Helpers para a fila (privadas)
	void enqueueMidiEvent(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t noteLength, bool isNoteOn);

	// Task do worker que consome a fila
	static void midiWorkerTask(void* pvParameters);
	
	// Para agendar Note Offs (uma fila por track para polifonia)
	struct PendingNoteOff {
		bool pending;
		uint8_t note;
		uint8_t channel;
		uint8_t velocity;  // Velocity para NOTE OFF
		uint16_t noteLength;  // Duração da nota em ms
		uint32_t time;
	};
	PendingNoteOff pendingNoteOffs[8];  // Uma entrada por track
	
	// Rastreia o último step euclidiano processado para cada track
	uint8_t lastProcessedStep[8] = {0};  // Uma entrada por track
	
	// Helper: enviar mensagem completa para saídas baseado em OutputProtocol
	void sendMidiMessage(uint8_t status, uint8_t data1, uint8_t data2, 
	                      EuclideanSequencer::OutputProtocol out);
	void sendRealTimeMessage(uint8_t msg, EuclideanSequencer::OutputProtocol out);
	
	// Callback interno: dispara notas quando sequenciador pede
	void onSequencerNote(uint8_t status, uint8_t data1, uint8_t data2);
	
public:
	EuclideanMidiEngine() = default;
	
	// Inicializa engine com refs para sequenciador, clock e interfaces MIDI
	void begin(EuclideanSequencer* seq, MidiClock* clk,
	           cs::BluetoothMIDI_Interface* ble = nullptr,
	           Adafruit_USBD_MIDI* usb = nullptr);
	
	// Define referência para OSCController (opcional)
	void setOSCController(OSCController* oscCtrl) { osc = oscCtrl; }
	
	// Chamada a cada loop: processa note-off e atualiza display
	void update();
	
	// Processa fila MIDI (USB)
	// Pode ser chamada múltiplas vezes por loop para evitar overflow
	void processMidiQueue();
	
	// Registra callbacks com o clock e sequenciador
	void registerClockCallbacks();
	
	// Métodos públicos para callbacks (não devem ser chamados diretamente, apenas via wrappers)
	void sendClockTick();
	void sendClockStart();
	void sendClockStop();
	void onStepStart(uint8_t step);
	
	// Controle de Play/Stop (encapsula envio de 0xFA/0xFC para todas as saídas)
	void sendPlayState(bool start);
	
	// Getters para debug MIDI
	uint32_t getMidiDroppedEvents() const { return midiDroppedEvents; }
	void resetDroppedEventCounter() { midiDroppedEvents = 0; }
	uint16_t getMidiQueueSize() const {
		return (midiQueueHead >= midiQueueTail) ? 
			(midiQueueHead - midiQueueTail) : 
			(MIDI_QUEUE_SIZE - midiQueueTail + midiQueueHead);
	}
	bool isOSCClientConnected() const;  // Implementação em CPP que verifica OSCController
};

#endif // EUCLIDEAN_MIDI_ENGINE_H
