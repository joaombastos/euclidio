#ifndef ROUTING_MATRIX_H
#define ROUTING_MATRIX_H

#include <stdint.h>

struct RoutingMatrix {
  // 5 entradas: DIN1(0), DIN2(1), DIN3(2), BLE(3), USB(4)
  // 5 saídas:   DIN1(0), DIN2(1), DIN3(2), BLE(3), USB(4)
  bool route[5][5];

  void clearAll() {
    // Inicializa matriz completamente vazia - nenhuma conexão
    for (uint8_t i = 0; i < 5; ++i) {
      for (uint8_t j = 0; j < 5; ++j) {
        route[i][j] = false;
      }
    }
  }

  void toggle(uint8_t inIndex, uint8_t outIndex) {
    if (inIndex < 5 && outIndex < 5) route[inIndex][outIndex] = !route[inIndex][outIndex];
  }

  bool get(uint8_t inIndex, uint8_t outIndex) const {
    if (inIndex < 5 && outIndex < 5) return route[inIndex][outIndex];
    return false;
  }
};

#endif
