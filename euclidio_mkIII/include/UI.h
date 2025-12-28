#ifndef UI_H
#define UI_H

#include <U8g2lib.h>
#include "RoutingMatrix.h"

// Forward declarations
class EuclideanSequencer;
class MidiClock;

namespace UIStrings {
  static const char LABEL_IN[]   = "In";     // prefixo para entradas (curto para caber)
  static const char LABEL_OUT[]  = "Out";    // prefixo para saídas
}

namespace UIFonts {
  [[maybe_unused]] static const uint8_t *ROUTING = u8g2_font_micro_tr;  // Fonte pequena para matriz de roteamento
  [[maybe_unused]] static const uint8_t *EUCLIDEAN = u8g2_font_tom_thumb_4x6_tf;  // Fonte para modo euclidiano
}

namespace UILayout {
  static const uint8_t TITLE_X = 0;
  static const uint8_t TITLE_Y = 6;

  // Output labels: Out1, Out2, Out3 centrados
  static const uint8_t OUT_LABEL_X0 = 45;   // X da primeira coluna (deslocada mais para direita, centralizado)
  static const uint8_t OUT_LABEL_Y  = 12;   // Y dos labels de OUT
  static const uint8_t COL_SPACING  = 26;   // espaçamento entre colunas

  // Input labels: In1-5 (com espaço livre para não colidir com quadrados)
  static const uint8_t IN_LABEL_X   = 20;   // X dos labels de IN (deslocada mais para direita, centralizado)
  static const uint8_t IN_LABEL_Y0  = 19;   // Y da primeira linha
  static const uint8_t ROW_SPACING  = 9;    // espaçamento entre linhas

  // Células (quadrados) - deslocadas para a direita
  static const uint8_t CELL_X0      = 52;   // X da primeira célula (mais à direita para centralizar)
  static const uint8_t CELL_Y0      = 13;   // Y da primeira célula
  static const uint8_t CELL_SIZE    = 6;    // tamanho da célula quadrada
  static const uint8_t CURSOR_PAD   = 1;    // espessura da moldura do cursor

  // Layout euclidean
  namespace Euclidean {
    static const uint8_t CENTER_X = 85;
    static const uint8_t CENTER_Y = 32;
    static const uint8_t CIRCLE_RADIUS = 28;
    static const uint8_t STEP_DOT_RADIUS = 2;
    static const uint8_t STEP_HIGHLIGHT_RADIUS = 4;
    static const uint8_t POINTER_LENGTH = 6;
    static const uint8_t LIST_X = 0;
    static const uint8_t LIST_WIDTH = 58;
    static const uint8_t LIST_Y_START = 8;
    static const uint8_t LIST_LINE_HEIGHT = 7;
    static const uint8_t LIST_VISIBLE_LINES = 8;
    static const uint8_t LIST_CURSOR_HEIGHT = 7;
    static const uint8_t BPM_X = 90;
    static const uint8_t BPM_Y = 8;
  }
}

class EuclideanHarmonicSequencer;

class UIController {
public:
  void begin(U8G2 &display);
  void render(RoutingMatrix &routing);
  void handleInput(RoutingMatrix &routing);
  void renderEuclidean(EuclideanSequencer &seq, MidiClock &clock, bool paramEditMode);
  void renderHarmonic(EuclideanHarmonicSequencer &hseq, MidiClock &clock, bool paramEditMode, uint8_t selectedParam, bool chordEditActive, uint8_t chordEditIndex);
  void setSelection(uint8_t in, uint8_t out);
  void processEncoderRotation(int rotation);
  uint8_t selectedIn() const { return selIn; }
  uint8_t selectedOut() const { return selOut; }
  bool isSelectingIn() const { return selectInMode; }
  void setSelectingIn(bool val) { selectInMode = val; }

private:
  U8G2 *disp = nullptr;
  uint8_t selIn = 0;
  uint8_t selOut = 0;
  bool selectInMode = false; // false: OUT, true: IN

  enum ButtonEvent { BTN_NONE, BTN_CLICK, BTN_LONG };
  ButtonEvent readButtonEvent();

  // Helper methods for euclidean rendering
  void drawEuclideanParamList(EuclideanSequencer &seq, MidiClock &clock);
  void drawEuclideanCircle(EuclideanSequencer &seq, MidiClock &clock);
  void drawEuclideanHeader(MidiClock &clock);
  // Preset UI removed: drawPresetList and drawPresetModeSelection removed
};

#endif
