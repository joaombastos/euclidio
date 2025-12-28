#ifndef OSC_CONTROLLER_H
#define OSC_CONTROLLER_H

#include <IPAddress.h>
#include <OSCMessage.h>
#include <AsyncUDP.h>
#include <freertos/FreeRTOS.h>

class EuclideanSequencer;
class MidiClock;

// Estrutura para guardar info de clientes conectados
struct OSCClient {
    IPAddress ip;
    uint16_t port;
    unsigned long lastActivity;
    bool active;
};

class OSCController {
public:
    OSCController();
    void begin();
    void setEuclideanSequencer(EuclideanSequencer* seq) { euclideanSeq = seq; }
    void setMidiClock(MidiClock* clock) { midiClock = clock; }
    void broadcastP2P(OSCMessage &msg, IPAddress senderIP);
    void broadcastFeedback(OSCMessage &msg);  // Envia feedback para todos os clientes P2P
    void updateClientActivity(IPAddress clientIP, bool isP2P);
    uint8_t getActiveP2PClientCount() const;
    int8_t findOrAddP2PClient(IPAddress clientIP, uint16_t port);

private:
    static const char* SSID;
    static const char* PASSWORD;
    static const int LOCAL_PORT;  // Porta 8000 para receber e enviar
    static const int MAX_P2P_CLIENTS = 2;    // aumentar para 4 clientes P2P (reencaminhamento)
    OSCClient p2pClients[MAX_P2P_CLIENTS];
    EuclideanSequencer* euclideanSeq;
    MidiClock* midiClock;
    void removeInactiveClients();
    void processMessage(OSCMessage &msg, IPAddress remoteIP, uint16_t remotePort);
    friend class OSCCallbackHelper;
};

#endif // OSC_CONTROLLER_H