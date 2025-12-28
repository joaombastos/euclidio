#include "Encoder.h"
#include <U8g2lib.h>
#include <vector>
#include "EuclideanSequencer.h"
#include "EuclideanHarmonicSequencer.h"
#include "MidiCCMapping.h"
#include "MidiFeedback.h"
#include "OSCMapping.h"
#include "MidiClock.h"
#include "EuclideanAnimation.h"
#include "AppState.h"
#include "UI.h"
#include "PresetUI.h"
#include "PresetManager.h"

Encoder::Encoder(int clk, int dt, int sw)
  : clkPin(clk), dtPin(dt), swPin(sw),
    lastClkState(HIGH), lastDtState(HIGH), lastSwState(HIGH) {
}

void Encoder::begin() {
  pinMode(clkPin, INPUT_PULLUP);
  pinMode(dtPin, INPUT_PULLUP);
  pinMode(swPin, INPUT_PULLUP);
  
  lastClkState = digitalRead(clkPin);
  lastDtState = digitalRead(dtPin);
  lastSwState = digitalRead(swPin);
}

int Encoder::readRotation() {
  static unsigned long lastDetectionTime = 0;
  unsigned long now = millis();
  
  // Debounce: ignora mudanças muito rápidas (bouncing)
  // Valor um pouco maior para estabilizar o encoder mecânico
  const unsigned long ROTARY_DEBOUNCE_MS = 5;  // pode ser ajustado entre 5-10ms se necessário
  if (now - lastDetectionTime < ROTARY_DEBOUNCE_MS) {
    return 0;  // Ignora se passou menos de 3ms desde última detecção
  }
  
  int clkState = digitalRead(clkPin);
  int dtState = digitalRead(dtPin);

  // Detecta apenas borda DESCENDENTE em CLK (HIGH → LOW)
  if (lastClkState == HIGH && clkState == LOW) {
    lastDetectionTime = now;  // Marca tempo da detecção
    
    lastClkState = clkState;
    lastDtState = dtState;
    
    // Lê DT neste exato momento para determinar direção
    if (dtState == HIGH) {
      return 1;   // Direita: incrementa
    } else {
      return -1;  // Esquerda: decrementa
    }
  }

  // Guarda estados para próxima leitura
  lastClkState = clkState;
  lastDtState = dtState;

  return 0;  // Nenhum movimento detectado
}

bool Encoder::isButtonPressed() {
  return digitalRead(swPin) == LOW;
}

Encoder::ClickType Encoder::readClickType() {
  static bool wasPressed = false;
  static unsigned long pressStartTime = 0;
  static bool longPressReported = false;
  
  bool isPressed = isButtonPressed();
  unsigned long now = millis();
  
  if (isPressed && !wasPressed) {
    pressStartTime = now;
    wasPressed = true;
    longPressReported = false;
    return CLICK_NONE;
  }
  
  if (isPressed && wasPressed && !longPressReported) {
    unsigned long pressDuration = now - pressStartTime;
    if (pressDuration >= 1200) {
      longPressReported = true;
      return CLICK_LONG;
    }
  }
  
  if (!isPressed && wasPressed) {
    unsigned long pressDuration = now - pressStartTime;
    wasPressed = false;
    
    if (longPressReported) {
      longPressReported = false;
      return CLICK_NONE;
    }
    
    // Verifica se está dentro da janela de double-click
    if (now - lastClickTime < DOUBLE_CLICK_WINDOW) {
      clickCount++;
      if (clickCount >= 2) {
        clickCount = 0;
        lastClickTime = 0;
        return CLICK_BACKWARD;  // Double-click detectado
      }
      return CLICK_NONE;  // Aguardando terceiro clique
    } else {
      // Fora da janela de double-click - novo ciclo
      clickCount = 1;
      lastClickTime = now;
      return CLICK_FORWARD;
    }
  }
  
  return CLICK_NONE;
}

// --- Encoder handler functions (migradas de EncoderHandler.cpp) ---
// Extern globals defined in main.cpp
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern EuclideanSequencer euclSeq;
extern Encoder encoder;
extern EuclideanHarmonicSequencer harmonicSeq;
extern MidiClock midiClock;
extern bool paramEditMode;
extern volatile bool doubleClickProcessing;
extern uint8_t harmonicEditParam;
extern bool harmonicChordEditMode;
extern uint8_t harmonicChordEditIndex;
extern std::vector<uint8_t> harmonicAllowedDegrees;
extern unsigned long lastRenderTime;
extern UIController ui;
extern bool g_presetMenuActive;

void EncoderHandler_init() {
  // atualmente não há passos de init separados do encoder.begin() em main
}
void EncoderHandler_handle() {
  // Se um menu de preset está ativo, deixamos o próprio menu ler o encoder
  if (g_presetMenuActive) {
    return;
  }

  Encoder::ClickType clickType = encoder.readClickType();
  int rotation = encoder.readRotation();

  // MODE_ROUTING: long press entra no sequenciador com menu de Load/Default
  if (appMode == MODE_ROUTING) {
    if (clickType == Encoder::CLICK_LONG) {
      euclSeq.begin();
      appMode = MODE_SEQUENCER;
      paramEditMode = false;
      midiClock.reset();

      // Menu de carregamento de preset (antes da animação)
      if (PresetUI::showLoadMenu(u8g2)) {
        String presetName = PresetUI::selectPreset(u8g2, false);
        if (presetName.length() > 0) {
          // Carregar os dois sequenciadores com o mesmo nome de preset
          PresetManager::loadEuclideanPreset(&euclSeq, presetName.c_str());
          PresetManager::loadHarmonicPreset(&harmonicSeq, presetName.c_str());

          // Após carregar, garantir que começamos na track 1 em ambos os sequenciadores
          euclSeq.setSelectedPattern(0);      // track 1 (0-based)
          euclSeq.loadPatternConfig();
          harmonicSeq.setActiveTrack(1);      // track 1 (1-based)
        }
      }

      // Mostrar animação euclidiana ao entrar no sequenciador
      EuclideanAnimation::show(u8g2);

      // Enviar snapshot único de parâmetros por MIDI e OSC ao entrar no modo sequenciador
      MidiFeedback::sendAllFeedbackForced(&euclSeq, &midiClock);
      OSCMapping::sendAllFeedbackForced(&euclSeq, &midiClock);

      // Enviar também o snapshot forçado do sequenciador harmônico imediatamente
      MidiFeedback::sendAllHarmonicFeedbackForced(&harmonicSeq, &midiClock);
      OSCMapping::sendAllHarmonicFeedbackForced(&harmonicSeq, &midiClock);

      // Feedback MIDI de long press
      MidiFeedback::sendNote(MidiCCMapping::getNoteEncoderLongPress(), 127);
      return;
    }

    if (rotation != 0) {
      ui.processEncoderRotation(rotation);
    }
    return;
  }

  // MODE_SEQUENCER handling
  if (appMode == MODE_SEQUENCER) {
    if (clickType == Encoder::CLICK_LONG) {
      // Menu Save/No ao sair do sequenciador
      bool shouldSave = PresetUI::showSaveMenu(u8g2);
      if (shouldSave) {
        String presetName = PresetUI::selectPreset(u8g2, true);
        if (presetName.length() > 0) {
          PresetManager::saveEuclideanPreset(&euclSeq, presetName.c_str());
          PresetManager::saveHarmonicPreset(&harmonicSeq, presetName.c_str());
        }
      }

      euclSeq.begin();
      euclSeq.stop();
      harmonicSeq.resetToDefaults();
      harmonicSeq.stop();
      midiClock.stop();
      appMode = MODE_ROUTING;

      // Feedback MIDI de long press
      MidiFeedback::sendNote(MidiCCMapping::getNoteEncoderLongPress(), 127);
      return;
    }

    if (clickType == Encoder::CLICK_BACKWARD) {
      appMode = MODE_HARMONIC;
      paramEditMode = false;
      MidiFeedback::sendAllHarmonicFeedbackForced(&harmonicSeq, &midiClock);
      OSCMapping::sendAllHarmonicFeedbackForced(&harmonicSeq, &midiClock);
      return;
    }

    if (clickType == Encoder::CLICK_FORWARD) {
      paramEditMode = !paramEditMode;
      return;
    }

    if (rotation == 0) return;

    if (!paramEditMode) {
      if (rotation > 0) euclSeq.nextEditParam(); else euclSeq.prevEditParam();
    } else {
      extern void editParam(int8_t direction);
      editParam((int8_t)rotation);
    }
    // Após qualquer mudança de parâmetro no sequenciador rítmico,
    // reenviar o snapshot de feedback MIDI/OSC (event-based, não mais periódico)
    MidiFeedback::sendAllFeedback(&euclSeq, &midiClock);
    OSCMapping::sendAllFeedback(&euclSeq, &midiClock);
    return;
  }

  // MODE_HARMONIC handling
  if (appMode == MODE_HARMONIC) {
    if (clickType == Encoder::CLICK_LONG) {
      // Mesmo comportamento de saída que no sequenciador: Save/No + reset
      bool shouldSave = PresetUI::showSaveMenu(u8g2);
      if (shouldSave) {
        String presetName = PresetUI::selectPreset(u8g2, true);
        if (presetName.length() > 0) {
          PresetManager::saveEuclideanPreset(&euclSeq, presetName.c_str());
          PresetManager::saveHarmonicPreset(&harmonicSeq, presetName.c_str());
        }
      }

      euclSeq.begin();
      euclSeq.stop();
      harmonicSeq.resetToDefaults();
      harmonicSeq.stop();
      midiClock.stop();
      appMode = MODE_ROUTING;

      MidiFeedback::sendNote(MidiCCMapping::getNoteEncoderLongPress(), 127);
      return;
    }

    if (clickType == Encoder::CLICK_BACKWARD) {
      appMode = MODE_SEQUENCER;
      paramEditMode = false;
      harmonicEditParam = 0;
      MidiFeedback::sendAllFeedback(&euclSeq, &midiClock);
      OSCMapping::sendAllFeedback(&euclSeq, &midiClock);
      return;
    }

    if (clickType == Encoder::CLICK_FORWARD) {
      if (harmonicEditParam == 14) {
        paramEditMode = !paramEditMode;
        return;
      }
      if (harmonicEditParam > 14) {
        if (!harmonicChordEditMode) {
          harmonicChordEditMode = true;
          harmonicChordEditIndex = harmonicEditParam - 15;
          paramEditMode = true;
          lastRenderTime = 0;
          MidiFeedback::sendNote(MidiCCMapping::getNoteEncoderDoubleClick(), 127);
        } else {
          harmonicChordEditMode = false;
          paramEditMode = false;
          lastRenderTime = 0;
          MidiFeedback::sendNote(MidiCCMapping::getNoteEncoderDoubleClick(), 64);
        }
        return;
      }
      paramEditMode = !paramEditMode;
      return;
    }

    if (rotation == 0) return;

    if (!paramEditMode) {
      int nChords = harmonicSeq.getChordListSize();
      int totalParams = 15 + nChords;
      harmonicEditParam = (uint8_t)(((int)harmonicEditParam + (rotation > 0 ? 1 : -1) + totalParams) % totalParams);
      lastRenderTime = 0;
    } else {
      int dir = (rotation > 0) ? 1 : -1;
      if (harmonicEditParam < 15) {
        switch (harmonicEditParam) {
          case 0: {
            int curTrack = (int)harmonicSeq.getActiveTrackNumber();
            int newTrack = constrain(curTrack + dir, 1, (int)EuclideanHarmonicSequencer::MAX_TRACKS);
            harmonicSeq.setActiveTrack((uint8_t)newTrack);
            break;
          }
          case 1: {
            auto cur = harmonicSeq.getScaleType();
            int next = ((int)cur + dir + (int)EuclideanHarmonicSequencer::SCALE_COUNT) % (int)EuclideanHarmonicSequencer::SCALE_COUNT;
            harmonicSeq.setScaleType((EuclideanHarmonicSequencer::ScaleType)next);
            break;
          }
          case 2: {
            int t = (int)harmonicSeq.getTonic() + dir;
            if (t < 0) t += 12; if (t >= 12) t -= 12;
            harmonicSeq.setTonic((uint8_t)t);
            break;
          }
          case 3:
            harmonicSeq.setDistributionMode((EuclideanHarmonicSequencer::DistributionMode)((harmonicSeq.getDistributionMode() == EuclideanHarmonicSequencer::DIST_CHORDS) ? EuclideanHarmonicSequencer::DIST_NOTES : EuclideanHarmonicSequencer::DIST_CHORDS)); break;
          case 4: {
            bool cur = harmonicSeq.isActive();
            harmonicSeq.setActive(!cur);
            lastRenderTime = 0;
            MidiFeedback::sendAllFeedback(&euclSeq, &midiClock);
            OSCMapping::sendAllFeedback(&euclSeq, &midiClock);
            break;
          }
          case 5: {
            uint8_t cur = harmonicSeq.getResolutionIndex();
            int next = ((int)cur + dir + 3) % 3;
            harmonicSeq.setResolutionIndex((uint8_t)next);
            break;
          }
          case 6:
            harmonicSeq.setSteps(constrain((int)harmonicSeq.getSteps() + dir, 1, 32)); break;
          case 7:
            harmonicSeq.setHits(constrain((int)harmonicSeq.getHits() + dir, 1, 32)); break;
          case 8:
            harmonicSeq.setOffset((harmonicSeq.getOffset() + dir + harmonicSeq.getSteps()) % harmonicSeq.getSteps()); break;
          case 9:
            harmonicSeq.setPolyphony(constrain((int)harmonicSeq.getPolyphony() + dir, 1, (int)EuclideanHarmonicSequencer::MAX_POLYPHONY)); break;
          case 10:
            harmonicSeq.setMidiChannel(constrain((int)harmonicSeq.getMidiChannel() + dir, 0, 15)); break;
          case 11:
            harmonicSeq.setVelocity(constrain((int)harmonicSeq.getVelocity() + dir, 0, 127)); break;
          case 12:
            harmonicSeq.setBaseOctave((int8_t)constrain((int)harmonicSeq.getBaseOctave() + dir, -2, 2)); break;
          case 13:
            harmonicSeq.setNoteLength(constrain((int)harmonicSeq.getNoteLength() + dir*50, 50, 2000)); break;
          case 14: {
            int nChords = harmonicSeq.getChordListSize();
            int newN = constrain(nChords + dir, 1, 16);
            if (newN > nChords) {
              for (int i = nChords; i < newN; ++i) {
                uint8_t nextDeg = (nChords > 0) ? (harmonicSeq.getChordListItem(nChords-1)+1)%7 : 1;
                harmonicSeq.addChordDegree(nextDeg);
              }
            } else if (newN < nChords) {
              for (int i = nChords; i > newN; --i) {
                harmonicSeq.removeLastChord();
              }
            }
            break;
          }
        }
      } else {
        if (harmonicChordEditMode) {
          harmonicAllowedDegrees.clear();
          harmonicSeq.getAllowedDegrees(harmonicAllowedDegrees);
          if (harmonicSeq.getChordListSize() > 0 && !harmonicAllowedDegrees.empty()) {
            uint8_t cur = harmonicSeq.getChordListItem(harmonicChordEditIndex);
            int pos = -1;
            for (uint8_t k=0;k<harmonicAllowedDegrees.size();++k) if (harmonicAllowedDegrees[k] == cur) { pos = k; break; }
            if (pos == -1) pos = 0;
            int npos = (int)pos + dir;
            while (npos < 0) npos += harmonicAllowedDegrees.size();
            npos = npos % harmonicAllowedDegrees.size();
            harmonicSeq.setChordListItem(harmonicChordEditIndex, harmonicAllowedDegrees[npos]);
          }
        }
      }
    }
    MidiFeedback::sendAllHarmonicFeedback(&harmonicSeq, &midiClock);
    OSCMapping::sendAllHarmonicFeedback(&harmonicSeq, &midiClock);
    return;
  }
}
