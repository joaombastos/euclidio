#include "UI.h"
#include "Pinos.h"
#include "EuclideanSequencer.h"
#include "MidiClock.h"
#include <math.h>
#include "MidiFeedback.h"
#include "MidiCCMapping.h"
#include "EuclideanHarmonicSequencer.h"

void UIController::begin(U8G2 &display) {
  disp = &display;
  disp->setFont(UIFonts::ROUTING);
}

void UIController::render(RoutingMatrix &routing) {
  if (!disp) return;
  disp->clearBuffer();
  disp->setFont(UIFonts::ROUTING);
  // Desenhar matriz de roteamento MIDI (sem caixas BLE)
  for (uint8_t j = 0; j < 3; ++j) {
    char label[4];
    sprintf(label, "%s%u", UIStrings::LABEL_OUT, j+1);
    disp->drawStr(UILayout::OUT_LABEL_X0 + j*UILayout::COL_SPACING, UILayout::OUT_LABEL_Y, label);
  }
  for (uint8_t i = 0; i < 4; ++i) { // Apenas IN1, IN2, IN3, USB
    char inLabel[6];
    if (i < 3) {
      sprintf(inLabel, "%s%u", UIStrings::LABEL_IN, i+1);
    } else {
      sprintf(inLabel, "USB");
    }
    disp->drawStr(UILayout::IN_LABEL_X, UILayout::IN_LABEL_Y0 + i*UILayout::ROW_SPACING, inLabel);
    for (uint8_t j = 0; j < 3; ++j) {
      bool on = routing.get(i, j);
      uint8_t x = UILayout::CELL_X0 + j*UILayout::COL_SPACING;
      uint8_t y = UILayout::CELL_Y0 + i*UILayout::ROW_SPACING;
      uint8_t sz = UILayout::CELL_SIZE;
      if (on) disp->drawBox(x, y, sz, sz); else disp->drawFrame(x, y, sz, sz);
      if (i == selIn && j == selOut) {
        disp->drawFrame(x-UILayout::CURSOR_PAD, y-UILayout::CURSOR_PAD, sz+2*UILayout::CURSOR_PAD, sz+2*UILayout::CURSOR_PAD);
      }
    }
  }
  disp->sendBuffer();
}

void UIController::renderEuclidean(EuclideanSequencer &seq, MidiClock &clock, bool paramEditMode) {
  if (!disp) return;
  disp->clearBuffer();
  disp->setFont(u8g2_font_tom_thumb_4x6_tf);

  // (Removed) top-right track label: not shown for rhythmic sequencer
  
  // Preset UI removed — sempre desenhar parâmetros e círculo
  drawEuclideanParamList(seq, clock);
  drawEuclideanCircle(seq, clock);
  drawEuclideanHeader(clock);
  
  disp->sendBuffer();
}

void UIController::renderHarmonic(EuclideanHarmonicSequencer &hseq, MidiClock &clock, bool paramEditMode, uint8_t selectedParam, bool chordEditActive, uint8_t chordEditIndex) {
  if (!disp) return;
  disp->clearBuffer();
  disp->setFont(u8g2_font_tom_thumb_4x6_tf);

  // Top: simple header removed (no tempo display over steps)

  // Left: parameter list (nova ordem solicitada)
  // Parâmetros principais + acordes em uma única lista
  const char* paramNames[] = {"Track","Scale","Tone","Modo","Active","Res","Steps","Hits","Offset","Poly","Chnl","Vel","Oct","Len","Chord"};
  uint8_t nMainParams = 15;
  uint8_t nChords = hseq.getChordListSize();
  uint8_t totalParams = nMainParams + nChords;
  const uint8_t visibleLines = UILayout::Euclidean::LIST_VISIBLE_LINES;
  int8_t scrollOffset = 0;

  if (selectedParam >= totalParams) selectedParam = totalParams - 1;

  // Calcular offset de scroll para manter o parâmetro selecionado visível e, quando possível, centralizado
  if (totalParams <= visibleLines) {
    scrollOffset = 0;
  } else {
    int8_t center = visibleLines / 2;
    int8_t s = (int8_t)selectedParam - center;
    if (s < 0) s = 0;
    if (s + visibleLines > totalParams) s = totalParams - visibleLines;
    scrollOffset = s;
  }

  // Sempre mostrar a lista de parâmetros, incluindo acordes, para edição direta
  for (uint8_t i=0;i<visibleLines;i++) {
    uint8_t paramIdx = i + scrollOffset;
    if (paramIdx >= totalParams) break;
    uint8_t y = UILayout::Euclidean::LIST_Y_START + i * UILayout::Euclidean::LIST_LINE_HEIGHT;
    bool sel = (paramIdx == selectedParam);

    // Construir valor do parâmetro em string
    char valueStr[24] = "";
    const char* label = nullptr;
    if (paramIdx < nMainParams) {
      label = paramNames[paramIdx];
      switch(paramIdx) {
        case 0: { // Track
          sprintf(valueStr, "%d", hseq.getActiveTrackNumber());
          break;
        }
        case 1: {
          const char* scaleNames[] = {
            "Major", "Nat.Min", "Harm.Min", "Mel.Min",
            "Ionian", "Dorian", "Phrygian", "Lydian",
            "Mixolydian", "Aeolian", "Locrian", "Mix b9 b13",
            "Lydian #9", "Phryg.Dominant", "Lydian #5", "Half-Whole",
            "Whole-Half", "Augmented", "Altered", "Pent. Maj",
            "Pent. Min", "Blues", "Gypsy", "Japanese",
            "Hirajoshi", "Kumoi", "Arabic"
          };
          int idx = (int)hseq.getScaleType();
          if (idx < 0) idx = 0; if (idx >= (int)(sizeof(scaleNames)/sizeof(scaleNames[0]))) idx = 0;
          sprintf(valueStr, "%s", scaleNames[idx]);
          break;
        }
        case 2: {
          const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
          sprintf(valueStr, "%s", noteNames[hseq.getTonic() % 12]);
          break;
        }
        case 3: sprintf(valueStr, "%s", hseq.getDistributionMode() == EuclideanHarmonicSequencer::DIST_CHORDS ? "Acordes" : "Notas"); break;
        case 4: sprintf(valueStr, "%s", hseq.isActive() ? "On" : "Off"); break;
        case 5: { const char* rnames[] = {"1/4","1/8","1/16"}; int ridx = (int)hseq.getResolutionIndex(); if (ridx < 0) ridx = 0; if (ridx > 2) ridx = 1; sprintf(valueStr, "%s", rnames[ridx]); } break;
        case 6: sprintf(valueStr, "%d", hseq.getSteps()); break;
        case 7: sprintf(valueStr, "%d", hseq.getHits()); break;
        case 8: sprintf(valueStr, "%d", hseq.getOffset()+1); break;
        case 9: sprintf(valueStr, "%d", hseq.getPolyphony()); break;
        case 10: sprintf(valueStr, "%d", hseq.getMidiChannel()+1); break;
        case 11: sprintf(valueStr, "%d", hseq.getVelocity()); break;
        case 12: sprintf(valueStr, "%+d", hseq.getBaseOctave()); break;
        case 13: sprintf(valueStr, "%d", hseq.getNoteLength()); break;
        case 14: sprintf(valueStr, "%d", hseq.getChordListSize()); break;
        default: sprintf(valueStr, ""); break;
      }
    } else {
      // Parâmetros de acordes/Notas individuais
      uint8_t chordIdx = paramIdx - nMainParams;
      static char chordName[24];
      if (hseq.getDistributionMode() == EuclideanHarmonicSequencer::DIST_NOTES) {
        // Apenas nome da nota (mapear grau para nota usando a escala)
        const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        uint8_t deg = hseq.getChordListItem(chordIdx);
        uint8_t midi = hseq.scaleDegreeToMidi(deg, 0);
        snprintf(chordName, sizeof(chordName), "%s", noteNames[midi % 12]);
      } else {
        // Nome completo do acorde + extensões conforme poly
        uint8_t poly = hseq.getPolyphony();
        hseq.formatChordName(chordIdx, poly, chordName, sizeof(chordName));
      }
      label = chordName;
      valueStr[0] = '\0'; // Não mostrar valor numérico, só o nome
    }

    // Render sem caixa; valores próximos ao nome — seguir estilo do sequenciador rítmico
    if (sel) {
      disp->setFont(u8g2_font_6x10_tf);
    } else {
      disp->setFont(UIFonts::EUCLIDEAN);
    }
    // Desenhar nome e posicionar o valor logo após (sem espaçamento excessivo)
    const uint8_t labelX = 2;
    disp->setCursor(labelX, y);
    disp->print(label);
    int nameW = disp->getStrWidth(label);
    int valX = labelX + nameW + 4;
    disp->setCursor(valX, y);
    disp->print(valueStr);
  }

  // Calcular quantas linhas realmente foram desenhadas (pode ser < visibleLines)
  uint8_t drawnLines = (totalParams - scrollOffset) < visibleLines ? (totalParams - scrollOffset) : visibleLines;

  // Right: circular pattern (multi-track) and chord list
  uint8_t cx = UILayout::Euclidean::CENTER_X;
  uint8_t cy = UILayout::Euclidean::CENTER_Y;
  uint8_t radius = UILayout::Euclidean::CIRCLE_RADIUS;
  // Desenhar os steps da track selecionada (ativa na UI)
  uint8_t activeTrack = hseq.getActiveTrackNumber() - 1;
  uint8_t s = hseq.getSteps();
  if (s == 0) s = 1;
  for (uint8_t i=0;i<s;i++) {
    float angle = (2.0 * PI * i) / s - PI/2.0;
    int x = cx + radius * cos(angle);
    int y = cy + radius * sin(angle);
    // smaller dots for harmonic steps: filled when active, outline otherwise
    const uint8_t filledRadius = 2; // was 3
    const uint8_t outlineRadius = 1; // was 2
    if (hseq.getPatternBit(i)) {
      disp->drawDisc(x, y, filledRadius, U8G2_DRAW_ALL);
    } else {
      disp->drawCircle(x, y, outlineRadius, U8G2_DRAW_ALL);
    }
  }

  // Desenhar ponteiros/agulhas para cada track ativa (enabled)
  for (uint8_t t = 0; t < EuclideanHarmonicSequencer::MAX_TRACKS; ++t) {
    // Exibir apenas tracks ativas (enabled)
    if (!hseq.isTrackEnabled(t)) continue;
    uint8_t stepsT = hseq.getStepsForTrack(t);
    if (stepsT == 0) stepsT = 1;
    uint8_t curStep = hseq.getCurrentStepForTrack(t) % stepsT;
    float angle = (2.0 * PI * curStep) / stepsT - PI/2.0;
    int px = cx + radius * cos(angle);
    int py = cy + radius * sin(angle);
    // Ponteiro mais grosso para a track selecionada
    // draw pointer and small indicator at tip
    if (t == activeTrack) {
      disp->drawLine(cx, cy, px, py);
      disp->drawCircle(px, py, 3, U8G2_DRAW_ALL); // slightly smaller highlight
    } else {
      disp->drawLine(cx, cy, px, py);
      disp->drawCircle(px, py, 2, U8G2_DRAW_ALL);
    }
  }

  // Lista de acordes sempre visível, estática, logo abaixo dos parâmetros
  uint8_t listStartY = UILayout::Euclidean::LIST_Y_START + drawnLines * UILayout::Euclidean::LIST_LINE_HEIGHT + 2;
  uint8_t maxShow = 4;
  uint8_t chordCount = hseq.getChordListSize();
  uint8_t chordStart = 0;
  // Seleção circular
  if (chordCount > maxShow) {
    if (chordEditActive) {
      chordStart = (chordEditIndex + chordCount - 1) % chordCount;
    }
  }
  for (uint8_t i=0; i<maxShow; ++i) {
    if (i >= chordCount) break;
    uint8_t idx = (chordStart + i) % chordCount;
    char buf[32];
      if (hseq.getDistributionMode() == EuclideanHarmonicSequencer::DIST_NOTES) {
      // Apenas nome da nota (usar mapeamento de grau baseado na escala)
      const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
      uint8_t deg = hseq.getChordListItem(idx);
      uint8_t midi = hseq.scaleDegreeToMidi(deg, 0);
      snprintf(buf, sizeof(buf), "%s", noteNames[midi % 12]);
    } else {
      // Nome completo do acorde + extensões
      hseq.formatChordName(idx, hseq.getPolyphony(), buf, sizeof(buf));
    }
    char outLabel[32];
    snprintf(outLabel, sizeof(outLabel), "%u:%s", idx+1, buf);
    disp->setFont(u8g2_font_6x10_tf);
    bool isSelected = (chordEditActive && chordEditIndex == idx);
    uint8_t xLabel = isSelected ? 8 : 2;
    if (isSelected) {
      disp->setCursor(2, listStartY + i * UILayout::Euclidean::LIST_LINE_HEIGHT);
      disp->print('>');
    }
    disp->setCursor(xLabel, listStartY + i * UILayout::Euclidean::LIST_LINE_HEIGHT);
    disp->print(outLabel);
  }
  
  // Show current chord/note name in center of circle
  char centerName[24];
    if (hseq.getChordListSize() > 0) {
    // chordListPos points to the next chord to be played (advance occurs in triggerChord),
    // so display the previously played chord to reflect what is currently sounding.
    uint8_t clSize = hseq.getChordListSize();
    uint8_t posNext = hseq.getChordListPos();
    uint8_t posPrev = (clSize == 0) ? 0 : (uint8_t)((posNext + clSize - 1) % clSize);
    if (hseq.getDistributionMode() == EuclideanHarmonicSequencer::DIST_NOTES) {
      // Apenas nome da nota (usar mapeamento de grau baseado na escala)
      const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
      uint8_t deg = hseq.getChordListItem(posPrev);
      uint8_t midi = hseq.scaleDegreeToMidi(deg, 0);
      snprintf(centerName, sizeof(centerName), "%s", noteNames[midi % 12]);
    } else {
      // Nome completo do acorde + extensões
      hseq.formatChordName(posPrev, hseq.getPolyphony(), centerName, sizeof(centerName));
    }
    } else {
    // show root of current step
    uint8_t curStep = hseq.getCurrentStep() % (hseq.getSteps()==0?1:hseq.getSteps());
    // map step->degree for display: use degree = curStep % SCALE_LEN
    uint8_t curDeg = curStep % 7;
    if (hseq.getDistributionMode() == EuclideanHarmonicSequencer::DIST_NOTES) {
      const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
      uint8_t midi = hseq.scaleDegreeToMidi(curDeg, 0);
      snprintf(centerName, sizeof(centerName), "%s", noteNames[midi % 12]);
    } else {
      char tmp[24];
      hseq.formatChordName(curDeg % (hseq.getChordListSize()?hseq.getChordListSize():1), hseq.getPolyphony(), tmp, sizeof(tmp));
      snprintf(centerName, sizeof(centerName), "%s", tmp);
    }
  }
  // Draw center name
  disp->setFont(u8g2_font_6x10_tf);
  int nameX = cx - (int)strlen(centerName)*3;
  disp->setCursor((nameX>0)?nameX:cx-12, cy+4);
  disp->print(centerName);

  // Pointer
  // Global pointer intentionally disabled: never draw the global time needle

  disp->sendBuffer();
}

void UIController::drawEuclideanHeader(MidiClock &clock) {
  // BPM display removed - kept for future use if needed
}

void UIController::drawEuclideanParamList(EuclideanSequencer &seq, MidiClock &clock) {
  // Parâmetros em ciclo único
  const char* allParams[] = {"Play", "Trk", "Dub", "Ch", "Note", "NtLen", "Vel", "Res", "Stps", "Hits", "Ofst", "Tmpo"};
  uint8_t currentParam = seq.getCurrentEditParam();
  uint8_t totalParams = 12;
  const char** currentParamList = allParams;
  
  const uint8_t visibleLines = UILayout::Euclidean::LIST_VISIBLE_LINES;
  int8_t scrollOffset = 0;

  // Calcula offset de scroll para manter o parâmetro atual visível e centrado
  uint8_t paramIdxAtual = currentParam;
  if (totalParams <= visibleLines) {
    scrollOffset = 0;
  } else {
    int8_t center = visibleLines / 2;
    scrollOffset = paramIdxAtual - center;
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset + visibleLines > totalParams) {
      scrollOffset = totalParams - visibleLines;
    }
  }

  for (uint8_t i = 0; i < visibleLines; ++i) {
    uint8_t paramIdx = i + scrollOffset;
    if (paramIdx >= totalParams) break;

    uint8_t y = UILayout::Euclidean::LIST_Y_START + i * UILayout::Euclidean::LIST_LINE_HEIGHT;
    bool selected = (paramIdxAtual == paramIdx);

    EuclideanSequencer::EditParam actualParam = (EuclideanSequencer::EditParam)paramIdx;

    // Construir string de valor
    const char* paramName = currentParamList[paramIdx];
    char valueStr[16] = "";
    
    switch (actualParam) {
      case EuclideanSequencer::PARAM_PLAY:
        sprintf(valueStr, "%s", seq.getPlayModeName(seq.getPlayMode()));
        break;
      case EuclideanSequencer::PARAM_TRACK:
        sprintf(valueStr, "%d", (int)seq.getSelectedTrackNumber());
        break;
      case EuclideanSequencer::PARAM_DUB:
        sprintf(valueStr, "%s", seq.isTrackEnabled(seq.getSelectedPattern()) ? "On" : "Off");
        break;
      case EuclideanSequencer::PARAM_MIDI_CHANNEL:
        sprintf(valueStr, "%d", seq.getMidiChannel() + 1);
        break;
      case EuclideanSequencer::PARAM_NOTE:
        sprintf(valueStr, "%d", seq.getNote());
        break;
      case EuclideanSequencer::PARAM_VELOCITY:
        sprintf(valueStr, "%d", seq.getVelocity());
        break;
      case EuclideanSequencer::PARAM_RESOLUTION: {
        uint8_t trackRes = seq.getTrackResolution(seq.getSelectedPattern());
        if (trackRes == 1) sprintf(valueStr, "1/4");
        else if (trackRes == 2) sprintf(valueStr, "1/8");
        else if (trackRes == 3) sprintf(valueStr, "1/16");
        else if (trackRes == 4) sprintf(valueStr, "1/32");
        break;
      }
      case EuclideanSequencer::PARAM_STEPS:
        sprintf(valueStr, "%d", seq.getSteps());
        break;
      case EuclideanSequencer::PARAM_HITS:
        sprintf(valueStr, "%d", seq.getHits());
        break;
      case EuclideanSequencer::PARAM_OFFSET:
        // Mostrar offset como 1..N (user-facing), internamente é 0..N-1
        sprintf(valueStr, "%d", seq.getOffset() + 1);
        break;
      case EuclideanSequencer::PARAM_NOTE_LENGTH:
        sprintf(valueStr, "%d", seq.getNoteLength());
        break;
      case EuclideanSequencer::PARAM_TEMPO:
        if (clock.isSlave()) {
          sprintf(valueStr, "SLAVE");
        } else {
          sprintf(valueStr, "%d BPM", (int)clock.getBPM());
        }
        break;
        // Removed ClockIO/NoteOut/MidiMap/OSCMap display — outputs are always active
      default:
        break;
    }
    
    // Renderizar com fonte apropriada
    if (selected) {
      // Parâmetro selecionado: exibir "Nome: Valor" em fonte grande
      disp->setFont(u8g2_font_6x10_tf);
      disp->setCursor(2, y);
      disp->print(paramName);
      disp->print(":");
      disp->print(valueStr);
    } else {
      // Parâmetro não selecionado: exibir "Nome: Valor" em fonte pequena
      disp->setFont(UIFonts::EUCLIDEAN);
      disp->setCursor(2, y);
      disp->print(paramName);
      disp->print(":");
      disp->print(valueStr);
    }
  }
}

void UIController::drawEuclideanCircle(EuclideanSequencer &seq, MidiClock &clock) {
  uint8_t cx = UILayout::Euclidean::CENTER_X;
  uint8_t cy = UILayout::Euclidean::CENTER_Y;
  uint8_t radius = UILayout::Euclidean::CIRCLE_RADIUS;

  // Não desenhar o círculo central

  // Pontos para todos os steps/hits de todas as tracks
  for (uint8_t t = 0; t < 8; ++t) {
    // Skip tracks that are not active or explicitly disabled (dub off)
    if (!seq.isTrackActive(t) || !seq.isTrackEnabled(t)) continue;
    uint8_t tSteps = seq.getTrackSteps(t);
    if (tSteps == 0) continue;
    for (uint8_t i = 0; i < tSteps; i++) {
      float angle = (2.0 * PI * i) / tSteps - PI / 2.0;
      int x = cx + radius * cos(angle);
      int y = cy + radius * sin(angle);
      bool selectedTrack = (t == seq.getSelectedPattern());
      if (seq.getTrackPatternBit(t, i)) {
        // Hit: highlight if selected, dimmer if not
        if (selectedTrack) disp->drawDisc(x, y, 3, U8G2_DRAW_ALL); // stronger hit
        else disp->drawDisc(x, y, 1, U8G2_DRAW_ALL); // subdued hit
      } else {
        // Non-hit: small outline, slightly larger if selected
        if (selectedTrack) disp->drawCircle(x, y, 2, U8G2_DRAW_ALL);
        else disp->drawCircle(x, y, 1, U8G2_DRAW_ALL);
      }
    }

    // Formas geométricas: linhas conectando hits da track
    int firstHitIdx = -1, lastHitIdx = -1;
    int prevX = 0, prevY = 0;
    for (uint8_t i = 0; i < tSteps; i++) {
      if (seq.getTrackPatternBit(t, i)) {
        float angle = (2.0 * PI * i) / tSteps - PI / 2.0;
        int x = cx + radius * cos(angle);
        int y = cy + radius * sin(angle);
        if (firstHitIdx == -1) {
          firstHitIdx = i;
          prevX = x;
          prevY = y;
        } else {
          // thicker line for selected track
          if (t == seq.getSelectedPattern()) {
            disp->drawLine(prevX, prevY, x, y);
            disp->drawLine(prevX+1, prevY, x+1, y);
            disp->drawLine(prevX, prevY+1, x, y+1);
          } else {
            disp->drawLine(prevX, prevY, x, y);
          }
          prevX = x;
          prevY = y;
        }
        lastHitIdx = i;
      }
    }
    // Fechar a forma geométrica se houver mais de um hit
    if (firstHitIdx != -1 && lastHitIdx != -1 && firstHitIdx != lastHitIdx) {
      float angleFirst = (2.0 * PI * firstHitIdx) / tSteps - PI / 2.0;
      float angleLast = (2.0 * PI * lastHitIdx) / tSteps - PI / 2.0;
      int xFirst = cx + radius * cos(angleFirst);
      int yFirst = cy + radius * sin(angleFirst);
      int xLast = cx + radius * cos(angleLast);
      int yLast = cy + radius * sin(angleLast);
      if (t == seq.getSelectedPattern()) {
        disp->drawLine(xLast, yLast, xFirst, yFirst);
        disp->drawLine(xLast+1, yLast, xFirst+1, yFirst);
      } else {
        disp->drawLine(xLast, yLast, xFirst, yFirst);
      }
    }
  }

  // Ponteiros/agulho: uma linha por track
  if (seq.isRunningState()) {
    for (uint8_t t = 0; t < 8; ++t) {
      if (!seq.isTrackActive(t) || !seq.isTrackEnabled(t)) continue;
      uint8_t tSteps = seq.getTrackSteps(t);
      if (tSteps == 0) continue;
      // Corrige visualização: agulha começa no passo 0 (vertical/12h)
      uint8_t tCur = seq.getTrackCurrentStep(t) % tSteps;
      float a = (2.0 * PI * tCur) / tSteps - PI / 2.0;
      int px = cx + radius * cos(a);
      int py = cy + radius * sin(a);
      if (t == seq.getSelectedPattern()) {
        disp->drawLine(cx, cy, px, py);
        disp->drawLine(cx+1, cy, px+1, py);
        disp->drawCircle(px, py, 3, U8G2_DRAW_ALL); // larger tip for selected
      } else {
        disp->drawLine(cx, cy, px, py);
        disp->drawCircle(px, py, 2, U8G2_DRAW_ALL);
      }
    }
  }
}

void UIController::handleInput(RoutingMatrix &routing) {
  // Nota: encoder é processado globalmente em main.cpp
  // Aqui apenas processamos o botão
  switch (readButtonEvent()) {
    case BTN_CLICK:
      routing.toggle(selIn, selOut);
      {
        // Feedback via mapeamento de nota da matriz removido; apenas alterna localmente
      }
      render(routing);
      break;
    case BTN_LONG:
      selectInMode = !selectInMode;
      render(routing);
      break;
    case BTN_NONE:
      break;
  }
}

void UIController::setSelection(uint8_t in, uint8_t out) {
  selIn = in;
  selOut = out;
}

void UIController::processEncoderRotation(int rotation) {
  if (rotation != 0) {
    if (selectInMode) {
      selIn = (uint8_t)((int)selIn + rotation + 4) % 4;
    } else {
      selOut = (uint8_t)((int)selOut + rotation + 3) % 3;
    }
  }
}

UIController::ButtonEvent UIController::readButtonEvent() {
  static int last = HIGH;
  static unsigned long pressStart = 0;
  static unsigned long lastEvent = 0;
  unsigned long now = millis();
  
  // Debounce de botão: ignorar mudanças rápidas
  if ((now - lastEvent) < 20) return BTN_NONE;
  
  int cur = digitalRead(ENC_SW);
  ButtonEvent ev = BTN_NONE;
  
  if (cur != last) {
    lastEvent = now;
    last = cur;
    
    if (cur == LOW) {
      // Pressionado
      pressStart = now;
    } else {
      // Libertado
      unsigned long dur = now - pressStart;
      ev = (dur >= 600) ? BTN_LONG : BTN_CLICK;
    }
  }
  
  return ev;
}

// Preset UI removed


