#include "StartupAnimation.h"
#include <Arduino.h>

void StartupAnimation::show(U8G2 &u8g2) {
    const int FRAMES = 16;
    const int CX = 64;
    const int CY = 28;

    for (int f = 0; f < FRAMES; ++f) {
        u8g2.clearBuffer();

        // small rotating dots animation
        for (int i = 0; i < 8; ++i) {
            float angle = (f * 20 + i * 45) * 3.1415926f / 180.0f;
            int radius = 6 + (i * 2);
            int x = CX + (int)(cos(angle) * radius);
            int y = CY + (int)(sin(angle) * radius);
            u8g2.drawBox(x - 2, y - 2, 5, 5);
        }
        u8g2.sendBuffer();
    }
    
    const unsigned long ANIM_MS = 3000; // total duration of the animation loop (ms)
    const unsigned long FRAME_DELAY_MS = 60;
    unsigned long start = millis();
    int f = 0;
    
    // Time-based loop to ensure consistent duration
    while ((unsigned long)(millis() - start) < ANIM_MS) {
        u8g2.clearBuffer();
        
        // small rotating dots animation
        for (int i = 0; i < 8; ++i) {
            float angle = (f * 20 + i * 45) * 3.1415926f / 180.0f;
            int radius = 6 + (i * 2);
            int x = CX + (int)(cos(angle) * radius);
            int y = CY + (int)(sin(angle) * radius);
            u8g2.drawBox(x - 2, y - 2, 5, 5);
        }
        
        u8g2.sendBuffer();
        delay(FRAME_DELAY_MS);
        ++f;
    }

}
