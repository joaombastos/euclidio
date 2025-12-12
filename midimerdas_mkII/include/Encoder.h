#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>
#include "Pinos.h"

class Encoder {
private:
  int clkPin;
  int dtPin;
  int swPin;
  
  int lastClkState;
  int lastDtState;
  int lastSwState;
  
  static const unsigned long DEBOUNCE_DELAY = 50;
  
public:
  Encoder(int clk = ENC_CLK, int dt = ENC_DT, int sw = ENC_SW);
  
  void begin();
  
  // Lê rotação: retorna +1 ou -1 por tick
  int readRotation();
  
  bool isButtonPressed();
  
  // Detecção de múltiplos cliques
  enum ClickType { CLICK_NONE = 0, CLICK_FORWARD, CLICK_BACKWARD, CLICK_LONG };
  ClickType readClickType();
  
private:
  unsigned long lastClickTime = 0;
  uint8_t clickCount = 0;
  static const unsigned long DOUBLE_CLICK_WINDOW = 300;  // ms
};

#endif