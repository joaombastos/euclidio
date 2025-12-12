#ifndef MIDI_CLOCK_H
#define MIDI_CLOCK_H

#include <Arduino.h>

class MidiClock {
public:
  enum SyncMode {
    MASTER = 0,  // Gera clock interno (master)
    SLAVE = 1    // Espera por clock externo (slave)
  };

  // Clock IO selection (bitmask) - onde enviar/receber clock
  enum ClockIO : uint8_t {
    CLOCK_NONE = 0x00,
    CLOCK_USB  = 0x01,
    CLOCK_BLE  = 0x02,
    CLOCK_DIN  = 0x04
  };

private:
  static const uint8_t MIDI_CLOCK = 0xF8;
  static const uint8_t MIDI_START = 0xFA;
  static const uint8_t MIDI_STOP = 0xFC;
  static const uint8_t MIDI_CONTINUE = 0xFB;
  
  static const uint8_t PPQN_BASE = 24;      // 24 pulsos por quarter note (não muda)
  
  hw_timer_t* timerHandle;
  float bpm;
  uint32_t tickCount;
  uint8_t currentPPQN;                      // contador dentro de PPQN (0-23)
  uint8_t currentStep;                      // passo atual (0-15)
  uint8_t ticksPerStep;                     // quantos ticks disparam um step (configurável)
  bool isRunning;
  SyncMode syncMode;                        // MASTER ou SLAVE
  unsigned long lastExternalClockTime;      // timestamp do último clock externo recebido
  
  // Debug: jitter do clock externo
  unsigned long lastClockInterval;          // intervalo em ms entre clocks
  uint16_t minClockInterval;                // intervalo mínimo observado
  uint16_t maxClockInterval;                // intervalo máximo observado
  uint32_t clockCount;                      // contador de clocks recebidos (para debug)
  
  // Callbacks para enviar clock MIDI
  void (*onClockTick)() = nullptr;
  void (*onStart)() = nullptr;
  void (*onStop)() = nullptr;
  void (*onContinue)() = nullptr;
  void (*onStepStart)(uint8_t step) = nullptr;

  // Seleção de onde o clock é enviado/recebido (bitmask de ClockIO)
  ClockIO clockIO = CLOCK_USB; // default: USB


public:
  MidiClock();
  
  void begin(float initialBpm = 120.0);
  void setBPM(float bpm);
  float getBPM() const { return bpm; }
  void setTicksPerStep(uint8_t ticks) { ticksPerStep = ticks; }
  uint8_t getTicksPerStep() const { return ticksPerStep; }
  
  // Controle de modo de sincronização
  void setSyncMode(SyncMode mode) { syncMode = mode; }
  SyncMode getSyncMode() const { return syncMode; }
  bool isMaster() const { return syncMode == MASTER; }
  bool isSlave() const { return syncMode == SLAVE; }
  
  // Recebe clock MIDI externo (chamado pelo MIDIRouter)
  void receiveExternalClock();
  
  void start();
  void stop();
  void reset();
  
  bool isRunningState() const { return isRunning; }
  uint32_t getTickCount() const { return tickCount; }
  uint8_t getBeatCount() const { return tickCount / 24; }  // 24 ticks por beat
  uint8_t getCurrentStep() const { return currentStep; }
  uint8_t getCurrentPPQN() const { return currentPPQN; }
  
  // Callbacks para integração externa
  void setClockCallback(void (*callback)()) { onClockTick = callback; }
  void setStartCallback(void (*callback)()) { onStart = callback; }
  void setStopCallback(void (*callback)()) { onStop = callback; }
  void setContinueCallback(void (*callback)()) { onContinue = callback; }
  void setStepStartCallback(void (*callback)(uint8_t)) { onStepStart = callback; }

  // Acesso à seleção de Clock I/O (público)
  void setClockIO(ClockIO io) { clockIO = io; }
  ClockIO getClockIO() const { return clockIO; }
  // Testa se uma entrada (inIndex 0..4) é permitida como fonte de clock
  bool isClockSourceEnabled(uint8_t inIndex) const;
  
  // Getters para debug de jitter
  uint16_t getMinClockInterval() const { return minClockInterval; }
  uint16_t getMaxClockInterval() const { return maxClockInterval; }
  unsigned long getLastClockInterval() const { return lastClockInterval; }
  uint32_t getClockCount() const { return clockCount; }
  void resetJitterStats();  // Reset dos contadores para nova medição
  
  // Triggerados internamente pelo timer
  void tickHandler();
  void startHandler();
  void stopHandler();

  // Flags/contadores gerados no ISR e processados no contexto de loop
  volatile uint16_t pendingClockTicks = 0;
  volatile bool pendingStartFlag = false;
  volatile bool pendingStopFlag = false;

  // Task handle para processamento dedicado do clock (FreeRTOS)
  TaskHandle_t clockTaskHandle = nullptr;

  // Loop da task que processa ticks pendentes (chamada pela FreeRTOS task)
  void clockTaskLoop();

  // Processa envios pendentes gerados pelo ISR (deve ser chamado no contexto de loop())
  void processPendingRealTime();
};

// Instância global
extern MidiClock midiClock;

#endif
