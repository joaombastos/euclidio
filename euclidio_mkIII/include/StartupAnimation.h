#ifndef STARTUP_ANIMATION_H
#define STARTUP_ANIMATION_H

#include <U8g2lib.h>

class StartupAnimation {
public:
    // Mostra uma animação simples no display passado (bloqueante, curta)
    static void show(U8G2 &u8g2);
};

#endif // STARTUP_ANIMATION_H
