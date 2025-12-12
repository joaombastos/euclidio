#ifndef PINOS_H
#define PINOS_H

// ========================================
// CONFIGURAÇÃO DE PINOS - MIDITRIX2000
// ========================================

// Porta MIDI DIN 1
#define DIN1_RX 16
#define DIN1_TX 17

// Porta MIDI DIN 2
#define DIN2_RX 18
#define DIN2_TX 10

// Porta MIDI DIN 3
#define DIN3_RX 11
#define DIN3_TX 12


// ========================================
// VELOCIDADE MIDI (padrão: 31250 baud)
// ========================================
#define MIDI_BAUD_RATE 31250

// ========================================
// OLED I2C
// ========================================
#define OLED_SDA 4
#define OLED_SCL 5

// ENCODER ROTATIVO + BOTÃO
#define ENC_CLK 7
#define ENC_DT  8
#define ENC_SW  9


#endif