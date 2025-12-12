#include "Encoder.h"

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
  
  // Debounce: ignora mudanças muito rápidas (bouncing) - 3ms para máxima responsividade
  if (now - lastDetectionTime < 3) {
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
