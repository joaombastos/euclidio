#include "MIDIRouter.h"
#include "RoutingMatrix.h"
#include "EuclideanSequencer.h"
#include "MidiCCMapping.h"
#include "MidiClock.h"
#include "Pinos.h"
#include <HardwareSerial.h>
// BLE support removed

#include "EuclideanHarmonicSequencer.h"
#include <Adafruit_TinyUSB.h>

// Ponteiros para matriz de roteamento e interfaces
RoutingMatrix* MIDIRouter::routingMatrix = nullptr;
EuclideanSequencer* MIDIRouter::euclideanSeq = nullptr;
MidiClock* MIDIRouter::midiClock = nullptr;
Adafruit_USBD_MIDI* MIDIRouter::usb_midi = nullptr;

void MIDIRouter::begin() {
	// Configura pinos de entrada/saída
	pinMode(DIN1_RX, INPUT_PULLUP);
	pinMode(DIN2_RX, INPUT_PULLUP);
	pinMode(DIN3_RX, INPUT_PULLUP);
	pinMode(DIN1_TX, OUTPUT);
	pinMode(DIN2_TX, OUTPUT);
	pinMode(DIN3_TX, OUTPUT);
	
	// Inicia as portas seriais MIDI
	Serial1.begin(MIDI_BAUD_RATE, SERIAL_8N1, DIN1_RX, DIN1_TX, false, 256);
	Serial2.begin(MIDI_BAUD_RATE, SERIAL_8N1, DIN2_RX, DIN2_TX, false, 256);
	Serial.begin(MIDI_BAUD_RATE, SERIAL_8N1, DIN3_RX, DIN3_TX, false, 256);
}

void MIDIRouter::sendToOutput(uint8_t outIndex, uint8_t b) {
	switch (outIndex) {
		case 0: Serial1.write(b); break;
		case 1: Serial2.write(b); break;
		case 2: Serial.write(b); break;
		case 4: if (usb_midi) usb_midi->write(b); break;  // USB
		default: break;  // Casos inválidos ou BLE (3) ignorados silenciosamente
	}
}

// Buffer MIDI para cada entrada (reconstituição de mensagens)
static uint8_t midiBuffer[3][3];        // 3 entradas, max 3 bytes por mensagem
static uint8_t midiBufferLen[3] = {0};  // Número de bytes acumulados por entrada

void MIDIRouter::routeByteFromInput(uint8_t inIndex, uint8_t b) {
	if (!routingMatrix) return;
	
	// Detecção de status byte
	if (b & 0x80) {
		// É um status byte
		
		// RealTime messages (0xF8-0xFF) não usam status de canal e não afetam o parser
		if (b >= 0xF8) {
			// RealTime message - roteia imediatamente sem afetar buffer
			for (uint8_t outIndex = 0; outIndex < 5; ++outIndex) {
				if (routingMatrix->get(inIndex, outIndex)) {
					if (outIndex < 3) {
						sendToOutput(outIndex, b);
					} else if (outIndex == 4 && usb_midi) {
						usb_midi->write(b);
					}
				}
			}
			return;
		}
		
		// Status byte normal - inicia nova mensagem (descarta incompleta anterior)
		midiBuffer[inIndex][0] = b;
		midiBufferLen[inIndex] = 1;
		return;
	}
	
	// Data byte
	if (midiBufferLen[inIndex] == 0) {
		// Data byte sem status byte anterior - ignora
		return;
	}
	
	// Acumula data byte
	midiBuffer[inIndex][midiBufferLen[inIndex]] = b;
	midiBufferLen[inIndex]++;
	
	// Verifica se mensagem está completa
	uint8_t status = midiBuffer[inIndex][0];
	uint8_t msgType = status & 0xF0;
	bool msgComplete = false;
	
	if ((msgType >= 0x80 && msgType <= 0xBF) || (msgType >= 0xE0 && msgType <= 0xEF)) {
		// Mensagens que usam 2 dados
		msgComplete = (midiBufferLen[inIndex] == 3);
	} else if (msgType == 0xF0) {
		// System exclusive - não suportado por enquanto
		midiBufferLen[inIndex] = 0;
		return;
	}
	
	// Se mensagem está completa, valida e roteia
	if (msgComplete) {
		// Validação básica: bytes de dados devem estar no intervalo 0-127
		if ((midiBuffer[inIndex][1] & 0x80) == 0 && (midiBuffer[inIndex][2] & 0x80) == 0) {
			routeChannelMessage(inIndex, midiBuffer[inIndex][0], midiBuffer[inIndex][1], midiBuffer[inIndex][2]);
		}
		// Sempre limpa o buffer após tentar rotear (completa ou inválida)
		midiBufferLen[inIndex] = 0;
	}
}

void MIDIRouter::routeChannelMessage(uint8_t inIndex, uint8_t header, uint8_t data1, uint8_t data2) {
	if (!routingMatrix) return;
	
	// Processa Control Change via mapeamento centralizado
	uint8_t msgType = header & 0xF0;
	uint8_t channel = header & 0x0F;
	
	if (msgType == 0xB0 && euclideanSeq) {  // Control Change
		// Processa via mapeamento (apenas no canal 10)
		MidiCCMapping::processCC(channel, data1, data2, euclideanSeq, midiClock);

		// Não descartamos mensagens CC aqui — processei via mapeamento,
		// mas deixamos o roteamento seguir normalmente para encaminhar tudo.
	}
	
	// Processa Note On/Off para encoder MIDI mapping
	if ((msgType == 0x90 || msgType == 0x80) && euclideanSeq) {  // Note On / Note Off
		// Note On é 0x90 com velocity > 0, ou 0x80 é sempre Note Off
		// Também trata Note On com velocity 0 como Note Off
		bool isNoteOn = (msgType == 0x90) && (data2 > 0);
		MidiCCMapping::processNote(channel, data1, data2, isNoteOn, euclideanSeq);
		
		// Não descartamos notas de controle aqui — o roteador encaminha todas as notas.
	}
	
	for (uint8_t outIndex = 0; outIndex < 5; ++outIndex) {
		if (routingMatrix->get(inIndex, outIndex)) {
			bool protocolEnabled = false;
			if (outIndex < 3) {
				// DIN: protocolo 3
				protocolEnabled = true; // (midMapProtocol == 3);
			} else if (outIndex == 4) {
				// USB: protocolo 0
				protocolEnabled = true; //(midMapProtocol == 0);
			}
            
			if (!protocolEnabled) continue;  // Pula se protocolo desativado
            
			if (outIndex < 3) {
				// Saídas DIN: byte-by-byte
				sendToOutput(outIndex, header);
				sendToOutput(outIndex, data1);
				if ((msgType >= 0x80 && msgType <= 0xBF) || (msgType >= 0xE0 && msgType <= 0xEF)) {
					sendToOutput(outIndex, data2);
				}
			} else {
				// Saída USB: enviar mensagem completa
				if (outIndex == 4 && usb_midi) {
					usb_midi->write(header);
					usb_midi->write(data1);
					if ((msgType >= 0x80 && msgType <= 0xBF) || (msgType >= 0xE0 && msgType <= 0xEF)) {
						usb_midi->write(data2);
					}
				}
			}
		}
	}
}

void MIDIRouter::routeRealTimeMessage(uint8_t inIndex, uint8_t message) {
	// MIDI Clock (0xF8) - apenas sincroniza internamente quando a fonte é permitida
	if (message == 0xF8) {
		if (midiClock && midiClock->isSlave()) {
			// Só aceitar clock externo se o input for permitido pela seleção de Clock I/O
			if (midiClock->isClockSourceEnabled(inIndex)) {
				midiClock->receiveExternalClock();
			}
		}
		return;  // Clock não é roteado para outputs
	}
	
	// MIDI Start (0xFA) - inicia o sequenciador em modo SLAVE
	if (message == 0xFA) {
		if (midiClock && midiClock->isSlave()) {
			midiClock->start();
		}
		return;  // Start não é roteado para outputs quando em SLAVE
	}
	
	// MIDI Stop (0xFC) - para o sequenciador em modo SLAVE
	if (message == 0xFC) {
		if (midiClock && midiClock->isSlave()) {
			midiClock->stop();
		}
		return;  // Stop não é roteado para outputs quando em SLAVE
	}
	
	if (!routingMatrix) return;
	
	for (uint8_t outIndex = 0; outIndex < 5; ++outIndex) {
			if (routingMatrix->get(inIndex, outIndex)) {
				if (outIndex < 3) {
					sendToOutput(outIndex, message);
				} else if (outIndex == 4 && usb_midi) {
					usb_midi->write(message);
				}
			}
	}
}

// Envia mensagem real-time (Clock/Start/Stop) para os outputs configurados no MidiClock
void MIDIRouter::sendRealtimeToClockOutputs(uint8_t message) {
	if (!midiClock) return;
	MidiClock::ClockIO io = midiClock->getClockIO();

	// DIN outputs
	if ((io & MidiClock::CLOCK_DIN) != 0) {
		// Envia a todos os DINs (0..2)
		for (uint8_t out = 0; out < 3; ++out) {
			sendToOutput(out, message);
		}
	}


	// USB
	if ((io & MidiClock::CLOCK_USB) != 0) {
		if (usb_midi) {
			usb_midi->write(message);
		}
	}
}

void MIDIRouter::clockTickCallback() {
	sendRealtimeToClockOutputs(0xF8);
	extern EuclideanHarmonicSequencer harmonicSeq;
	harmonicSeq.update();
}

void MIDIRouter::startCallback() {
	sendRealtimeToClockOutputs(0xFA);
}

void MIDIRouter::stopCallback() {
	sendRealtimeToClockOutputs(0xFC);
}
