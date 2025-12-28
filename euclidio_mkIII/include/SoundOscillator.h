#ifndef SOUND_OSCILLATOR_H
#define SOUND_OSCILLATOR_H

#include <Arduino.h>
#include <ArduinoSound.h>
#include "EuclideanOscillator.h"

class SoundOscillator {
public:
    SoundOscillator();
    void begin();
    void play(const EuclideanOscillator& params);
    void stop();
    void update(const EuclideanOscillator& params);
    bool isPlaying() const;

private:
    // Exemplo: ponteiros para objetos de onda da ArduinoSound
    SineWaveModulated *sineOsc = nullptr;
    SquareWave *squareOsc = nullptr;
    TriangleWave *triangleOsc = nullptr;
    SawtoothWave *sawOsc = nullptr;
    // ... outros membros conforme necessário

    void setupOscillator(const EuclideanOscillator& params);
    void applyEnvelope(const Envelope& env);
};

#endif // SOUND_OSCILLATOR_H
