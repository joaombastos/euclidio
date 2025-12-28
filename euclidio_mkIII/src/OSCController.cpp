#include "OSCController.h"
#include "OSCMapping.h"
#include "EuclideanSequencer.h"
#include "MidiClock.h"
#include <WiFi.h>
#include <AsyncUDP.h>
#include <ESPmDNS.h>
#include <string.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Definições estáticas da classe
const char* OSCController::SSID = "ESP32_OSC";
const char* OSCController::PASSWORD = "12345678";
const int OSCController::LOCAL_PORT = 8000;

// Variável global UDP para receber e enviar (reutilizada)
static AsyncUDP udp;

// Instância global do OSCController (para acessar em callback)
static OSCController* g_osc_controller = nullptr;

// Classe Print customizada para capturar bytes OSC
class ByteBuffer : public Print {
private:
	uint8_t buffer[256];
	size_t pos;
	static const size_t MAX_SIZE = 256;
public:
	ByteBuffer() : pos(0) {}
	
	size_t write(uint8_t c) override {
		if (pos < MAX_SIZE) {
			buffer[pos++] = c;
			return 1;
		}
		return 0;
	}
	
	uint8_t* getBuffer() { return buffer; }
	size_t getSize() { return pos; }
	bool isFull() { return pos >= MAX_SIZE; }
};

OSCController::OSCController() 
	: euclideanSeq(nullptr), midiClock(nullptr) {
	g_osc_controller = this;
	// Inicializa arrays de clientes P2P
	for (int i = 0; i < MAX_P2P_CLIENTS; i++) {
		p2pClients[i].ip = IPAddress(0, 0, 0, 0);
		p2pClients[i].port = 0;
		p2pClients[i].lastActivity = 0;
		p2pClients[i].active = false;
	}
}

// Funções de envio de evento OSC do sequenciador removidas

int8_t OSCController::findOrAddP2PClient(IPAddress clientIP, uint16_t port) {
	// Procura cliente P2P existente
	for (int i = 0; i < MAX_P2P_CLIENTS; i++) {
		if (p2pClients[i].active && p2pClients[i].ip == clientIP) {
			p2pClients[i].lastActivity = millis();
			return i;
		}
	}
	
	// Procura slot vazio para novo cliente P2P
	for (int i = 0; i < MAX_P2P_CLIENTS; i++) {
		if (!p2pClients[i].active) {
			p2pClients[i].ip = clientIP;
			p2pClients[i].port = port;
			p2pClients[i].lastActivity = millis();
			p2pClients[i].active = true;
			return i;
		}
	}
	
	// Se não houver slot livre, substitui o cliente menos ativo (LRU)
	int oldestIdx = 0;
	unsigned long oldestTime = ULONG_MAX;
	for (int i = 0; i < MAX_P2P_CLIENTS; i++) {
		if (p2pClients[i].lastActivity < oldestTime) {
			oldestTime = p2pClients[i].lastActivity;
			oldestIdx = i;
		}
	}

	// Substituir o slot mais antigo
	p2pClients[oldestIdx].ip = clientIP;
	p2pClients[oldestIdx].port = port;
	p2pClients[oldestIdx].lastActivity = millis();
	p2pClients[oldestIdx].active = true;
	return oldestIdx;
}

void OSCController::removeInactiveClients() {
	const unsigned long TIMEOUT_MS = 120000; // 2 minutos
	unsigned long now = millis();
	for (int i = 0; i < MAX_P2P_CLIENTS; i++) {
		if (p2pClients[i].active) {
			if ((now - p2pClients[i].lastActivity) > TIMEOUT_MS) {
				p2pClients[i].active = false;
				p2pClients[i].ip = IPAddress(0,0,0,0);
				p2pClients[i].port = 0;
				p2pClients[i].lastActivity = 0;
			}
		}
	}
}

uint8_t OSCController::getActiveP2PClientCount() const {
	uint8_t count = 0;
	for (int i = 0; i < MAX_P2P_CLIENTS; i++) {
		if (p2pClients[i].active) count++;
	}
	return count;
}

void OSCController::updateClientActivity(IPAddress clientIP, bool isP2P) {
	if (isP2P) {
		for (int i = 0; i < MAX_P2P_CLIENTS; i++) {
			if (p2pClients[i].active && p2pClients[i].ip == clientIP) {
				p2pClients[i].lastActivity = millis();
				return;
			}
		}
	}
}

void OSCController::begin() {
	// Proteção contra múltiplas inicializações
	static bool initialized = false;
	if (initialized) {
		return;
	}
	
	// Inicia WiFi em modo Access Point
	WiFi.softAP(SSID, PASSWORD);
	delay(100);
	
	// Inicia mDNS para descoberta automática
	if (!MDNS.begin("Euclidio")) {
		// Se falhar, continua mesmo assim
	}
	// Anuncia o serviço OSC na rede
	MDNS.addService("osc", "udp", LOCAL_PORT);
	
	// Listener UDP na porta 8000
	if (udp.listen(LOCAL_PORT)) {
		udp.onPacket([](AsyncUDPPacket packet) {
			if (!g_osc_controller) {
				return;
			}
			
			if (packet.length() > 0 && packet.length() < 512) {
				OSCMessage msg;
				msg.fill(packet.data(), packet.length());
				
				if (!msg.hasError()) {
					g_osc_controller->processMessage(msg, packet.remoteIP(), packet.remotePort());
				}
			}
		});
		initialized = true;

		// Criação de fila e task para envio OSC removida
	}
}

// oscSendTask removido (não há mais fila/eventos OSC do sequenciador)

void OSCController::broadcastP2P(OSCMessage &msg, IPAddress senderIP) {
	// Reencaminha mensagem para TODOS os clientes P2P EXCETO o que enviou
	ByteBuffer bb;
	msg.send(bb);
	
	if (bb.getSize() > 0 && !bb.isFull()) {
		for (int i = 0; i < MAX_P2P_CLIENTS; i++) {
			if (p2pClients[i].active && p2pClients[i].ip != senderIP) {
				udp.writeTo(bb.getBuffer(), bb.getSize(), p2pClients[i].ip, LOCAL_PORT);
			}
		}
	}
}

void OSCController::broadcastFeedback(OSCMessage &msg) {
	// Envia feedback para TODOS os clientes P2P (sem exceção)
	ByteBuffer bb;
	msg.send(bb);
	
	if (bb.getSize() > 0 && !bb.isFull()) {
		bool anyClient = false;
		for (int i = 0; i < MAX_P2P_CLIENTS; i++) {
			if (p2pClients[i].active) {
				anyClient = true;
				udp.writeTo(bb.getBuffer(), bb.getSize(), p2pClients[i].ip, LOCAL_PORT);
			}
		}
		// Fallback: se não houver clientes registados ainda, enviar broadcast na rede AP
		if (!anyClient) {
			udp.writeTo(bb.getBuffer(), bb.getSize(), IPAddress(255, 255, 255, 255), LOCAL_PORT);
		}
	}
}

void OSCController::processMessage(OSCMessage &msg, IPAddress remoteIP, uint16_t remotePort) {
	// Limpa clientes inativos periodicamente
	static unsigned long lastCleanup = 0;
	if (millis() - lastCleanup > 5000) {
		removeInactiveClients();
		lastCleanup = millis();
	}
	
	if (msg.size() < 1) {
		return;
	}
	
	// Obtém o path da mensagem
	char addressBuffer[64];
	msg.getAddress(addressBuffer);
	
	// Verifica se é uma mensagem de controle do sequenciador
	bool isControlMessage = false;
	if (euclideanSeq && strncmp(addressBuffer, "/sequencer/", 11) == 0) {
		isControlMessage = true;
	} else if (strncmp(addressBuffer, "/encoder/", 9) == 0) {
		isControlMessage = true;
	} else if (strncmp(addressBuffer, "/routing/", 10) == 0) {
		isControlMessage = true;
	} else if (strncmp(addressBuffer, "/ui/matrix/", 12) == 0) {
		// matrix enter/exit control
		isControlMessage = true;
	} else if (strncmp(addressBuffer, "/harmonic", 9) == 0) {
		// Harmonic sequencer control messages (e.g. /harmonic/steps)
		isControlMessage = true;
	}
	
	// Registrar cliente P2P mesmo que tenha enviado mensagem de controle
	// Isso permite que dispositivos (ex: TouchOSC) que só enviam comandos
	// de controle também recebam feedback OSC.
	findOrAddP2PClient(remoteIP, remotePort);
	updateClientActivity(remoteIP, true);

	// Se for mensagem de controle, processa mas não reencaminha
	if (isControlMessage && euclideanSeq) {
		// Other control messages are handled via OSCMapping below.

		// For other control messages, reuse OSCMapping (numeric args)
		float args[4];
		int argc = 0;
		for (int i = 0; i < msg.size() && i < 4; i++) {
			if (msg.getType(i) == 'f' || msg.getType(i) == 'i') {
				if (msg.getType(i) == 'f') {
					args[argc++] = msg.getFloat(i);
				} else {
					args[argc++] = (float)msg.getInt(i);
				}
			}
		}

		// Processa via OSCMapping
		OSCMapping::processMessage(addressBuffer, argc, args, euclideanSeq, midiClock);
		return;  // Não reencaminha mensagens de controle
	}
	
	// Todas as outras mensagens são tratadas como P2P
	// Registra como cliente P2P
	findOrAddP2PClient(remoteIP, remotePort);
	updateClientActivity(remoteIP, true);
	// Reencaminha para outros clientes P2P
	broadcastP2P(msg, remoteIP);
}

