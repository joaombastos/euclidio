#include "PresetUI.h"
#include "Encoder.h"
#include "PresetManager.h"
#include "UI.h"  // para UIFonts

extern bool g_presetMenuActive;

// Instância global do encoder definida em main.cpp
extern Encoder encoder;

bool PresetUI::showLoadMenu(U8G2 &u8g2) {
    // Menu simples: Load (esquerda) vs Default (direita)
    // Navegação com encoder
    bool selected = false;  // false = Load, true = Default
    bool confirmed = false;

    g_presetMenuActive = true;

    // Ignora o long press inicial: espera o botão ser libertado
    while (encoder.isButtonPressed()) {
        encoder.readRotation();
        delay(10);
        yield();
    }

    while (!confirmed) {
        // Ler encoder
        int rotation = encoder.readRotation();
        if (rotation > 0) {
            selected = true;   // Default
        } else if (rotation < 0) {
            selected = false;  // Load
        }

        // Verificar botão
        if (encoder.isButtonPressed()) {
            confirmed = true;
        }

        // Renderizar menu (fonte maior e simples)
        u8g2.clearBuffer();
        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_6x10_tf);

        // Título
        u8g2.drawStr(2, 12, "Load preset?");

        // Opções (Load e Default) com seta '>'
        if (!selected) {
            u8g2.drawStr(2, 30, "> Load");
            u8g2.drawStr(2, 45, "  Default");
        } else {
            u8g2.drawStr(2, 30, "  Load");
            u8g2.drawStr(2, 45, "> Default");
        }

        u8g2.sendBuffer();
        delay(50);  // 50ms para não ir muito rápido
        yield();
    }
    // Restaurar cor padrão antes de sair
    u8g2.setDrawColor(1);
    g_presetMenuActive = false;

    return !selected;  // retorna true se Load foi selecionado (selected == false)
}

bool PresetUI::showSaveMenu(U8G2 &u8g2) {
    // Menu simples: Save (esquerda) vs No (direita)
    bool selected = false;  // false = Save, true = No
    bool confirmed = false;

    g_presetMenuActive = true;

    // Ignora o long press inicial: espera o botão ser libertado
    while (encoder.isButtonPressed()) {
        encoder.readRotation();
        delay(10);
        yield();
    }

    while (!confirmed) {
        // Ler encoder
        int rotation = encoder.readRotation();
        if (rotation > 0) {
            selected = true;   // No
        } else if (rotation < 0) {
            selected = false;  // Save
        }

        // Verificar botão
        if (encoder.isButtonPressed()) {
            confirmed = true;
        }

        // Renderizar menu simples
        u8g2.clearBuffer();
        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_6x10_tf);

        // Título
        u8g2.drawStr(2, 12, "Save preset?");

        if (!selected) {
            u8g2.drawStr(2, 30, "> Save");
            u8g2.drawStr(2, 45, "  No");
        } else {
            u8g2.drawStr(2, 30, "  Save");
            u8g2.drawStr(2, 45, "> No");
        }

        u8g2.sendBuffer();
        delay(50);  // 50ms para não ir muito rápido
        yield();
    }
    u8g2.setDrawColor(1);
    g_presetMenuActive = false;

    return !selected;  // retorna true se Save foi selecionado (selected == false)
}

String PresetUI::selectPreset(U8G2 &u8g2, bool isSave) {
    // Listar presets disponíveis do SD (para saber existentes)
    String presetList[32];
    String* presetPtrs[32];
    for (int i = 0; i < 32; i++) {
        presetPtrs[i] = &presetList[i];
    }
    
    uint8_t count = 0;
    bool success = PresetManager::listEuclideanPresets(presetPtrs, 32, count);
    if (!success) {
        return "";
    }

    g_presetMenuActive = true;

    // Ignora o estado de botão ainda pressionado antes de entrar na seleção
    while (encoder.isButtonPressed()) {
        encoder.readRotation();
        delay(10);
        yield();
    }

    // Caso SAVE: mostrar todos os ficheiros (para poder sobrescrever) + opção "novo" automática
    if (isSave) {
        // Gerar nome novo do tipo preset01..preset99 que não exista ainda
        String newName;
        for (int i = 1; i <= 99; ++i) {
            char buf[12];
            sprintf(buf, "preset%02d", i);
            bool exists = false;
            for (uint8_t j = 0; j < count; ++j) {
                if (presetList[j].equalsIgnoreCase(buf)) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                newName = String(buf);
                break;
            }
        }
        if (newName.length() == 0) {
            newName = "preset99";  // fallback improvável
        }

        // Construir lista de exibição: primeiro item = novo preset, restantes = presets existentes (exceto "default")
        String displayList[33];
        uint8_t displayCount = 0;
        displayList[displayCount++] = newName;
        for (uint8_t j = 0; j < count && displayCount < 33; ++j) {
            if (presetList[j].equalsIgnoreCase("default")) continue; // não mostrar default no SAVE
            displayList[displayCount++] = presetList[j];
        }

        int selected = 0;
        bool confirmed = false;

        while (!confirmed) {
            int rotation = encoder.readRotation();
            if (rotation > 0) {
                selected = (selected + 1) % displayCount;
            } else if (rotation < 0) {
                selected = (selected - 1 + displayCount) % displayCount;
            }

            if (encoder.isButtonPressed()) {
                confirmed = true;
            }

            u8g2.clearBuffer();
            u8g2.setDrawColor(1);
            u8g2.setFont(u8g2_font_6x10_tf);

            u8g2.drawStr(2, 12, "Save as:");

            // Numero maximo de linhas realmente visiveis no ecrã
            const uint8_t maxShow = 4;
            uint8_t start = 0;
            if (displayCount > maxShow) {
                // Tentar manter o item selecionado aproximadamente no meio da janela
                int8_t center = maxShow / 2; // 3 para maxShow=6
                int16_t s = selected - center;
                if (s < 0) s = 0;
                if (s + maxShow > displayCount) s = displayCount - maxShow;
                start = (uint8_t)s;
            }

            for (uint8_t i = 0; i < maxShow && (start + i) < displayCount; ++i) {
                uint8_t idx = start + i;
                uint8_t y = 28 + i * 10;
                // Marcar o primeiro item (novo preset) com sufixo "(new)" para ficar claro
                String label = displayList[idx];
                char buf[24];
                if (idx == 0) {
                    snprintf(buf, sizeof(buf), "%s*", label.c_str());
                } else {
                    snprintf(buf, sizeof(buf), "%s", label.c_str());
                }

                if (idx == (uint8_t)selected) {
                    u8g2.drawStr(2, y, "> ");
                    u8g2.drawStr(10, y, buf);
                } else {
                    u8g2.drawStr(2, y, "  ");
                    u8g2.drawStr(10, y, buf);
                }
            }

            u8g2.sendBuffer();
            delay(50);
            yield();
        }

        u8g2.setDrawColor(1);
        g_presetMenuActive = false;

        // selected == 0 => novo preset; >0 => preset existente (incluindo default), que será sobrescrito
        return displayList[selected];
    }

    // Caso LOAD: lista completa, incluindo "default"
    if (count == 0) {
        g_presetMenuActive = false;
        return "";
    }

    int selected = 0;
    bool confirmed = false;

    while (!confirmed) {
        int rotation = encoder.readRotation();
        if (rotation > 0) {
            selected = (selected + 1) % count;
        } else if (rotation < 0) {
            selected = (selected - 1 + count) % count;
        }

        if (encoder.isButtonPressed()) {
            confirmed = true;
        }

        // Renderizar lista simples para LOAD
        u8g2.clearBuffer();
        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_6x10_tf);

        u8g2.drawStr(2, 12, "Load:");

        // Numero maximo de linhas realmente visiveis no ecrã
        const uint8_t maxShow = 4;
        uint8_t start = 0;
        if (count > maxShow) {
            int8_t center = maxShow / 2;
            int16_t s = selected - center;
            if (s < 0) s = 0;
            if (s + maxShow > count) s = count - maxShow;
            start = (uint8_t)s;
        }

        for (uint8_t i = 0; i < maxShow && (start + i) < count; ++i) {
            uint8_t idx = start + i;
            uint8_t y = 28 + i * 10;
            if (idx == (uint8_t)selected) {
                u8g2.drawStr(2, y, "> ");
                u8g2.drawStr(10, y, presetList[idx].c_str());
            } else {
                u8g2.drawStr(2, y, "  ");
                u8g2.drawStr(10, y, presetList[idx].c_str());
            }
        }

        u8g2.sendBuffer();
        delay(50);
        yield();
    }

    u8g2.setDrawColor(1);
    g_presetMenuActive = false;
    return presetList[selected];
}
