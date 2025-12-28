// Minimal stub to replace Control Surface library
// Provides no-op BLE-MIDI interface and ChannelMessage so project compiles
#ifndef NO_CONTROL_SURFACE_H
#define NO_CONTROL_SURFACE_H

#include <cstdint>

namespace cs {
    class ChannelMessage {
    public:
        ChannelMessage(uint8_t a=0, uint8_t b=0, uint8_t c=0) {}
    };

    class BluetoothMIDI_Interface {
    public:
        BluetoothMIDI_Interface() {}
        void begin() {}
        void send(const ChannelMessage&) {}
        void sendNow() {}
        void sendRealTime(uint8_t) {}
        void setName(const char*) {}
    };
}


// Provide global aliases so existing unqualified uses keep compiling
using BluetoothMIDI_Interface = cs::BluetoothMIDI_Interface;
using ChannelMessage = cs::ChannelMessage;

// Also provide a minimal Control_Surface facade with begin/loop no-ops
namespace Control_Surface {
    inline void begin() {}
    inline void loop() {}
}

#endif // NO_CONTROL_SURFACE_H
