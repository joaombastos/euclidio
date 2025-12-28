#include "EuclideanAnimation.h"

void EuclideanAnimation::show(U8G2 &u8g2) {
    const unsigned long DURATION_MS = 2500; // duração total ~2.5s
    const unsigned long FRAME_DELAY_MS = 42;
    const int CX = 64;
    const int CY = 28;
    unsigned long start = millis();
    int frame = 0;

    // Garantir que desenhamos em branco (foreground)
    u8g2.setDrawColor(1);

    while ((unsigned long)(millis() - start) < DURATION_MS) {
        u8g2.clearBuffer();

        // pulsing concentric circles
        int radius = 8 + (frame % 6) * 4;
        u8g2.drawCircle(CX, CY, radius);
        u8g2.drawCircle(CX, CY, radius/2);

        // rotating small squares
        for (int i = 0; i < 6; ++i) {
            float angle = (frame * 15 + i * 60) * 3.1415926f / 180.0f;
            int x = CX + (int)(cos(angle) * (radius + 6));
            int y = CY + (int)(sin(angle) * (radius + 6));
            u8g2.drawBox(x-2, y-2, 5, 5);
        }
        u8g2.sendBuffer();
        delay(FRAME_DELAY_MS);
        ++frame;
    }
}
