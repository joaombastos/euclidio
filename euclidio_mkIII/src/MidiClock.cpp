#include "MidiClock.h"
#include "MIDIRouter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// Handler estático para o timer da ESP32
static void IRAM_ATTR onTimerTick() {
  midiClock.tickHandler();
}

// Wrapper para a FreeRTOS task
static void clockTaskWrapper(void* pv) {
  MidiClock* clk = (MidiClock*)pv;
  clk->clockTaskLoop();
  vTaskDelete(NULL);
}

MidiClock::MidiClock()
  : timerHandle(nullptr), bpm(120.0), tickCount(0), currentPPQN(0), currentStep(0), ticksPerStep(6), isRunning(false), syncMode(MASTER), lastExternalClockTime(0), lastClockInterval(0), minClockInterval(65535), maxClockInterval(0), clockCount(0) {
}

bool MidiClock::isClockSourceEnabled(uint8_t inIndex) const {
  // inIndex: 0..2 = DIN1..DIN3, 3 = BLE, 4 = USB
  switch (inIndex) {
    case 0:
    case 1:
    case 2:
      return (clockIO & CLOCK_DIN) != 0;
    case 3:
      return (clockIO & CLOCK_BLE) != 0;
    case 4:
      return (clockIO & CLOCK_USB) != 0;
    default:
      return false;
  }
}

void MidiClock::begin(float initialBpm) {
  bpm = initialBpm;
  tickCount = 0;
  isRunning = false;
  
  // Cria timer usando o timer 0 da ESP32 (com frequência de 80MHz)
  // Intervalo = (1000000 / (BPM/60 * 24)) microsegundos por tick
  // BPM 120 = 2 beats/segundo = 48 ticks/segundo = ~20833 microsegundos
  
  timerHandle = timerBegin(0, 80, true);  // Timer 0, prescaler 80 (1MHz)
  timerAttachInterrupt(timerHandle, &onTimerTick, true);
  timerAlarmWrite(timerHandle, 20833, true);  // Valor padrão para 120 BPM

  // Criar uma task dedicada para processar clock (pinning no Core 1)
  if (!clockTaskHandle) {
    // Prioridade máxima para garantir latência mínima do clock
    UBaseType_t maxPriority = configMAX_PRIORITIES - 1;
    xTaskCreatePinnedToCore(clockTaskWrapper, "MidiClockTask", 4096, this, maxPriority, &clockTaskHandle, 1);
  }
}

void MidiClock::setBPM(float newBpm) {
  if (newBpm > 0) {
    bpm = newBpm;
    
    // Recalcula o intervalo do timer
    // 1MHz clock (1 microsegundo por tick)
    // Ticks necessários por segundo = BPM / 60 * 24
    // Microsegundos por tick = 1000000 / (BPM/60 * 24)
    uint32_t intervalUs = (uint32_t)(1000000.0 / (bpm / 60.0 * 24.0));
    
    if (timerHandle) {
      timerAlarmWrite(timerHandle, intervalUs, true);
    }
  }
}

void MidiClock::start() {
  if (!isRunning) {
    tickCount = 0;
    currentPPQN = 0;
    currentStep = 0;
    isRunning = true;
    if (timerHandle && syncMode == MASTER) {
      timerAlarmEnable(timerHandle);
    }
    if (onStart) {
      onStart();
    }
  }
}

void MidiClock::stop() {
  if (isRunning && timerHandle) {
    isRunning = false;
    timerAlarmDisable(timerHandle);
    
    if (onStop) {
      onStop();
    }
  }
}

void MidiClock::reset() {
  tickCount = 0;
  currentPPQN = 0;
  currentStep = 0;
}

void MidiClock::tickHandler() {
  // Em modo SLAVE, não usar timer interno
  if (syncMode == SLAVE) {
    return;
  }

  // Minimizar trabalho no ISR: apenas marca ticks pendentes para processamento
  if (isRunning) {
    pendingClockTicks++;
    // Notificar a task dedicada (se criada) de forma segura para ISR
    if (clockTaskHandle) {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      vTaskNotifyGiveFromISR(clockTaskHandle, &xHigherPriorityTaskWoken);
      if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    }
  }
}

void MidiClock::receiveExternalClock() {
  // Função chamada quando recebe 0xF8 (Clock) de uma porta MIDI
  uint32_t now = micros();

  // Calcula intervalo desde o último clock (em micros para melhor resolução)
  if (lastExternalClockTime > 0) {
    lastClockInterval = (uint32_t)(now - lastExternalClockTime);

    // Atualiza min/max para detecção de jitter (converte para ms-range values if needed)
    if (lastClockInterval < minClockInterval) {
      minClockInterval = (uint16_t)lastClockInterval;
    }
    if (lastClockInterval > maxClockInterval) {
      maxClockInterval = (uint16_t)lastClockInterval;
    }
  }

  lastExternalClockTime = now;
  clockCount++;

  // Minimizar trabalho: marcar tick pendente e processar no loop
  if (syncMode == SLAVE && isRunning) {
    pendingClockTicks++;
    // Notificar a task dedicada (se criada)
    if (clockTaskHandle) {
      xTaskNotifyGive(clockTaskHandle);
    }
  }
}

void MidiClock::startHandler() {
  // marcar start pendente para processamento no loop
  start();
  pendingStartFlag = true;
}

void MidiClock::stopHandler() {
  // marcar stop pendente para processamento no loop
  stop();
  pendingStopFlag = true;
}

void MidiClock::processPendingRealTime() {
  // Leitura atômica das flags/counters
  noInterrupts();
  uint16_t ticks = pendingClockTicks;
  bool hasStart = pendingStartFlag;
  bool hasStop = pendingStopFlag;
  pendingClockTicks = 0;
  pendingStartFlag = false;
  pendingStopFlag = false;
  interrupts();

  // Enviar Start/Stop/Clock através do MIDIRouter (contexto seguro)
  if (hasStart) {
    MIDIRouter::sendRealtimeToClockOutputs(0xFA);
  }
  if (hasStop) {
    MIDIRouter::sendRealtimeToClockOutputs(0xFC);
  }

  // Processar todos os clock ticks pendentes: atualizar estado interno
  // e notificar callbacks no contexto de loop (seguro para I/O)
  for (uint16_t i = 0; i < ticks; ++i) {
    // Atualiza contadores internos
    tickCount++;
    currentPPQN++;

    // Atualiza passo a cada ticksPerStep ticks PPQN
    if (currentPPQN >= ticksPerStep) {
      currentPPQN = 0;
      currentStep++;
      if (onStepStart) onStepStart(currentStep);
    }

    // Callback de clock (seguro no contexto de loop)
    if (onClockTick) onClockTick();

    // NOTE: envio para saídas de clock é feito pelo callback `onClockTick`
    // (ex.: `MIDIRouter::clockTickCallback`) para evitar duplicação.
  }
}

void MidiClock::clockTaskLoop() {
  // Task principal: espera notificação da ISR e processa ticks pendentes
  for (;;) {
    // Aguarda notificação (ulTaskNotifyTake decrementa o contador de notificações)
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // Processar quaisquer ticks/start/stop pendentes de forma atômica
    processPendingRealTime();
  }
}

void MidiClock::resetJitterStats() {
  minClockInterval = 65535;
  maxClockInterval = 0;
  clockCount = 0;
  lastClockInterval = 0;
}
