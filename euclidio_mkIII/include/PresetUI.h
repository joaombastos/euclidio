#ifndef PRESET_UI_H
#define PRESET_UI_H

#include <Arduino.h>
#include <U8g2lib.h>

class EuclideanSequencer;
class EuclideanHarmonicSequencer;
class Encoder;

extern Encoder encoder;

class PresetUI {
public:
    // Menu de carregamento (Load/Default) - retorna true se Load foi selecionado
    static bool showLoadMenu(U8G2 &u8g2);
    
    // Menu de gravação (Save/No) - retorna true se Save foi selecionado
    static bool showSaveMenu(U8G2 &u8g2);
    
    // Listar e selecionar um preset (retorna o nome ou vazio se cancelado)
    static String selectPreset(U8G2 &u8g2, bool isHarmonic);
};

#endif // PRESET_UI_H
