#ifndef MIDI_ROUTER_H
#define MIDI_ROUTER_H

#include <stdint.h>

class RoutingMatrix;
class EuclideanSequencer;
class MidiClock;

// Forward declarations com namespace
namespace cs {
	class BluetoothMIDI_Interface;
}
class Adafruit_USBD_MIDI;

class MIDIRouter {
public:
	// Saídas MIDI: 0=DIN1, 1=DIN2, 2=DIN3, 3=BLE, 4=USB
	
	// Inicializa os pinos de saída serial MIDI e referencia BLE/USB
	static void begin();
	static void setBleInterface(cs::BluetoothMIDI_Interface* ble) { ble_midi = ble; }
	static void setUsbInterface(Adafruit_USBD_MIDI* usb) { usb_midi = usb; }
	
	// Envia um byte para uma saída MIDI específica
	static void sendToOutput(uint8_t outIndex, uint8_t b);
	
	// Roteia um byte simples de uma entrada para saídas habilitadas
	static void routeByteFromInput(uint8_t inIndex, uint8_t b);
	
	// Roteia uma mensagem MIDI completa (header + 2 dados)
	static void routeChannelMessage(uint8_t inIndex, uint8_t header, uint8_t data1, uint8_t data2);
	
	// Roteia mensagens em tempo real (Clock, Start, Stop, etc)
	static void routeRealTimeMessage(uint8_t inIndex, uint8_t message);

	// Callbacks para enviar Start/Stop/Clock a saídas selecionadas
	static void sendRealtimeToClockOutputs(uint8_t message);
	static void clockTickCallback();
	static void startCallback();
	static void stopCallback();
	
	// Define a matriz de roteamento a usar
	static void setRoutingMatrix(RoutingMatrix* matrix) { routingMatrix = matrix; }
	
	// Define sequenciador para controle via MIDI CC
	static void setEuclideanSequencer(EuclideanSequencer* seq) { euclideanSeq = seq; }
	
	// Define MidiClock para sincronização de play/stop via MIDI CC
	static void setMidiClock(MidiClock* clock) { midiClock = clock; }

private:
	static RoutingMatrix* routingMatrix;
	static EuclideanSequencer* euclideanSeq;
	static MidiClock* midiClock;
	static cs::BluetoothMIDI_Interface* ble_midi;
	static Adafruit_USBD_MIDI* usb_midi;
};

#endif // MIDI_ROUTER_H
