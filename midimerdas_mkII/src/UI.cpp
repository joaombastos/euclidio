#include "UI.h"
#include "Pinos.h"
#include "EuclideanSequencer.h"
#include "MidiClock.h"
#include <math.h>
#include "MidiFeedback.h"
#include "MidiCCMapping.h"

void UIController::begin(U8G2 &display) {
  disp = &display;
  disp->setFont(UIFonts::ROUTING);
}

void UIController::render(RoutingMatrix &routing) {
  if (!disp) return;
  disp->clearBuffer();
  disp->setFont(UIFonts::ROUTING);
  for (uint8_t j = 0; j < 3; ++j) {
    char label[4];
    sprintf(label, "%s%u", UIStrings::LABEL_OUT, j+1);
    disp->drawStr(UILayout::OUT_LABEL_X0 + j*UILayout::COL_SPACING, UILayout::OUT_LABEL_Y, label);
  }
  for (uint8_t i = 0; i < 5; ++i) {
    char inLabel[6];
    if (i < 3) {
      sprintf(inLabel, "%s%u", UIStrings::LABEL_IN, i+1);
    } else if (i == 3) {
      sprintf(inLabel, "BLE");
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
  
  // Preset UI removed — sempre desenhar parâmetros e círculo
  drawEuclideanParamList(seq, clock);
  drawEuclideanCircle(seq, clock);
  drawEuclideanHeader(clock);
  
  disp->sendBuffer();
}

void UIController::drawEuclideanHeader(MidiClock &clock) {
  // BPM display removed - kept for future use if needed
}

void UIController::drawEuclideanParamList(EuclideanSequencer &seq, MidiClock &clock) {
  // Dois grupos de parâmetros com navegação separada
  const char* groupMainParams[] = {"Play", "Trk", "Dub", "Ch", "Note", "NtLen", "Vel", "Res", "Stps", "Hits", "Ofst"};
  const char* groupOutputParams[] = {"Tmpo", "ClkIO", "NoteOut", "MidiMap", "OSCMap"};
  
  uint8_t currentParam = seq.getCurrentEditParam();
  uint8_t currentGroup = seq.getCurrentGroup();
  // buffer removido (não usado)
  
  const char** currentParamList;
  uint8_t totalParams;
  
  // Selecionar qual grupo mostrar
  if (currentGroup == 0) {
    currentParamList = groupMainParams;
    totalParams = 11;  // Play, Trk, Dub, Ch, Note, NtLen, Vel, Res, Stps, Hits, Ofst
  } else {
    currentParamList = groupOutputParams;
    totalParams = 5;  // Tmpo, ClkIO, NoteOut, MidiMap, OSCMap
  }
  
  const uint8_t visibleLines = UILayout::Euclidean::LIST_VISIBLE_LINES;
  int8_t scrollOffset = 0;
  
  // Calcula offset de scroll para manter o parâmetro atual visível e centrado
  uint8_t paramInGroup;
  if (currentGroup == 0) {
    paramInGroup = currentParam;  // 0-6
  } else {
    paramInGroup = currentParam - 100;  // 0-3
  }
  
  // Evitar offsets negativos quando totalParams < visibleLines
  if (totalParams <= visibleLines) {
    scrollOffset = 0;
  } else {
    int8_t center = visibleLines / 2;
    scrollOffset = paramInGroup - center;
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset + visibleLines > totalParams) {
      scrollOffset = totalParams - visibleLines;
    }
  }

  for (uint8_t i = 0; i < visibleLines; ++i) {
    uint8_t paramIdx = i + scrollOffset;
    if (paramIdx >= totalParams) break;
    
    uint8_t y = UILayout::Euclidean::LIST_Y_START + i * UILayout::Euclidean::LIST_LINE_HEIGHT;
    bool selected = (paramInGroup == paramIdx);
    
    // Map paramIdx ao enum actual baseado no grupo
    EuclideanSequencer::EditParam actualParam;
    if (currentGroup == 0) {
      actualParam = (EuclideanSequencer::EditParam)paramIdx;  // 0-9
    } else {
      actualParam = (EuclideanSequencer::EditParam)(paramIdx + 100);  // 100-102
    }
    
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
        case EuclideanSequencer::PARAM_CLOCK_IO: {
          // Display clock IO selection
          auto io = clock.getClockIO();
          const char* name = "None";
          bool usb = (io & MidiClock::CLOCK_USB);
          bool ble = (io & MidiClock::CLOCK_BLE);
          bool din = (io & MidiClock::CLOCK_DIN);
          if (usb && !ble && !din) name = "USB";
          else if (!usb && ble && !din) name = "BLE";
          else if (!usb && !ble && din) name = "DIN";
          else if (usb && ble && !din) name = "USB+BLE";
          else if (usb && !ble && din) name = "USB+DIN";
          else if (!usb && ble && din) name = "BLE+DIN";
          else name = "None";
          sprintf(valueStr, "%s", name);
          break;
        }
      case EuclideanSequencer::PARAM_NOTE_OUT: {
          auto outs = seq.getOutputNotes();
          const char* name = "None";
          bool usb = (outs & EuclideanSequencer::OUT_USB);
          bool ble = (outs & EuclideanSequencer::OUT_BLE);
          bool din = (outs & EuclideanSequencer::OUT_DIN);
          if (usb && !ble && !din) name = "USB";
          else if (!usb && ble && !din) name = "BLE";
          else if (!usb && !ble && din) name = "DIN";
          else if (usb && ble && !din) name = "USB+BLE";
          else if (usb && !ble && din) name = "USB+DIN";
          else if (!usb && ble && din) name = "BLE+DIN";
          else if (usb && ble && din) name = "ALL";
          else name = "None";
          sprintf(valueStr, "%s", name);
          break;
        }
          case EuclideanSequencer::PARAM_MIDI_MAP: {
            bool m = seq.getOutputMidiMap();
            sprintf(valueStr, "%s", m ? "On" : "Off");
            break;
          }
          case EuclideanSequencer::PARAM_OSC_MAP: {
            bool o = seq.getOutputOSCMap();
            sprintf(valueStr, "%s", o ? "On" : "Off");
            break;
          }
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
      selIn = (uint8_t)((int)selIn + rotation + 5) % 5;
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


