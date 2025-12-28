#ifndef APP_STATE_H
#define APP_STATE_H

#include <vector>
#include <cstdint>

enum AppMode {
    MODE_ROUTING,
    MODE_SEQUENCER,
    MODE_HARMONIC
};

extern AppMode appMode;
extern AppMode lastAppMode;

extern bool paramEditMode;
extern volatile bool doubleClickProcessing;
extern volatile bool oscEncoderDoubleClickRequested;
extern uint8_t harmonicEditParam;
extern bool harmonicChordEditMode;
extern uint8_t harmonicChordEditIndex;
extern std::vector<uint8_t> harmonicAllowedDegrees;

extern unsigned long lastRenderTime;
extern uint8_t lastDisplayedParam;
extern uint8_t lastDisplayedGroup;

// Flag global para indicar que um menu de preset modal está ativo
extern bool g_presetMenuActive;

#endif // APP_STATE_H
