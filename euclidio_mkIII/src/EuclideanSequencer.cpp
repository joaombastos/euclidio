#include "EuclideanSequencer.h"
#include "Encoder.h"

EuclideanSequencer::EuclideanSequencer()
  : currentStep(0), selectedPattern(0), isRunning(false), 
    lastStepTime(0), currentEditParam(PARAM_PLAY), 
    outputNotes(OUT_ALL), outputClock(OUT_ALL), outputMidiMap(true), outputOSCMap(true) {
  pattern.reserve(MAX_STEPS);
  for (uint8_t i = 0; i < 8; ++i) trackCurrentStep[i] = 0;
}

void EuclideanSequencer::begin() {
  // Inicializa com valores padrão
  currentConfig.steps = DEFAULT_STEPS;
  currentConfig.hits = 4;
  currentConfig.offset = 0;
  currentConfig.note = 36;              // Note internal default
  currentConfig.velocity = 100;         // Velocidade padrão 100
  currentConfig.midiChannel = 1;        // Canal MIDI 2 (0-15, display como 1-16)
  currentConfig.resolution = 2;         // 2 = 1/8 note (resolution 1 = 1/4)
  currentConfig.noteLength = 100;       // Duração padrão da nota: 100ms (50-500)
  currentConfig.playMode = STOP;        // Modo de play padrão (parado)
  currentConfig.active = true;
  currentConfig.enabled = false;         // Dub OFF por padrão
  
  // Inicializa padrões salvos: slot 0 ativo com config atual
  for (uint8_t i = 0; i < MAX_PATTERNS; i++) {
    patterns[i].active = false;
    patterns[i].enabled = false; // Dub OFF por padrão em todas as tracks
  }
  patterns[0] = currentConfig;
  patterns[0].active = true;
  patterns[0].enabled = false;
  
  generatePattern();
}

void EuclideanSequencer::generatePattern() {
  pattern.clear();
  bjorklundAlgorithm(currentConfig.steps, currentConfig.hits);
  // Persistir no slot atual
  if (selectedPattern < MAX_PATTERNS) {
    patterns[selectedPattern] = currentConfig;
    patterns[selectedPattern].active = true;
  }
}

void EuclideanSequencer::bjorklundAlgorithm(uint8_t steps, uint8_t hits) {
  // Garantir que hits não excedem steps na geração
  if (hits > steps) hits = steps;
  if (steps == 0 || hits == 0) {
    for (uint8_t i = 0; i < steps; i++) {
      pattern.push_back(false);
    }
    return;
  }
  
  if (hits == steps) {
    for (uint8_t i = 0; i < steps; i++) {
      pattern.push_back(true);
    }
    return;
  }
  
  // Geração com ancoragem no passo 0:
  // Hit em i quando ((i * hits) % steps) < hits
  for (uint8_t i = 0; i < steps; i++) {
    bool isHit = (((uint16_t)i * hits) % steps) < hits;
    pattern.push_back(isHit);
  }
  
  // Garante o tamanho correto
  while (pattern.size() < steps) {
    pattern.push_back(false);
  }
  while (pattern.size() > steps) {
    pattern.pop_back();
  }

  // Aplica offset como rotação do padrão
  if (steps > 0 && currentConfig.offset % steps != 0) {
    std::vector<bool> rotated;
    rotated.reserve(steps);
    uint8_t off = currentConfig.offset % steps;
    for (uint8_t i = 0; i < steps; ++i) {
      uint8_t src = (i + steps - (off % steps)) % steps; // rotação para a direita por 'offset'
      rotated.push_back(pattern[src]);
    }
    pattern.swap(rotated);
  }
}

void EuclideanSequencer::start() {
  isRunning = true;
  // Inicia em steps-1 para sincronizar a agulha em 12h
  currentStep = (currentConfig.steps > 0) ? (currentConfig.steps - 1) : 0;
  lastStepTime = millis();
}

void EuclideanSequencer::stop() {
  isRunning = false;
  currentStep = 0;
}

void EuclideanSequencer::reset() {
  currentStep = 0;
  lastStepTime = millis();
}

void EuclideanSequencer::setSteps(uint8_t steps) {
  if (steps > 0 && steps <= MAX_STEPS) {
    currentConfig.steps = steps;
    if (selectedPattern < MAX_PATTERNS) {
      patterns[selectedPattern] = currentConfig;
      patterns[selectedPattern].active = true;
    }
    generatePattern();
  }
}

void EuclideanSequencer::setHits(uint8_t hits) {
  // Limitar entre 1 e MAX_HITS (agora permitindo pares também)
  if (hits == 0) hits = 1;
  if (hits > MAX_HITS) hits = MAX_HITS;
  currentConfig.hits = hits;
  if (selectedPattern < MAX_PATTERNS) {
    patterns[selectedPattern] = currentConfig;
    patterns[selectedPattern].active = true;
  }
  generatePattern();
}

void EuclideanSequencer::setOffset(uint8_t offset) {
  currentConfig.offset = offset % currentConfig.steps;
  if (selectedPattern < MAX_PATTERNS) {
    patterns[selectedPattern] = currentConfig;
    patterns[selectedPattern].active = true;
  }
  generatePattern();
}

void EuclideanSequencer::setNote(uint8_t note) {
  currentConfig.note = note;
  if (selectedPattern < MAX_PATTERNS) {
    patterns[selectedPattern] = currentConfig;
    patterns[selectedPattern].active = true;
  }
}

void EuclideanSequencer::setVelocity(uint8_t vel) {
  currentConfig.velocity = constrain(vel, 0, 127);
  if (selectedPattern < MAX_PATTERNS) {
    patterns[selectedPattern] = currentConfig;
    patterns[selectedPattern].active = true;
  }
}

void EuclideanSequencer::setMidiChannel(uint8_t channel) {
  if (channel < 16) {
    currentConfig.midiChannel = channel;
    if (selectedPattern < MAX_PATTERNS) {
      patterns[selectedPattern] = currentConfig;
      patterns[selectedPattern].active = true;
    }
  }
}

void EuclideanSequencer::setResolution(uint8_t res) {
  // res: 1=1/2, 2=1/4, 3=1/8, 4=1/16
  if (res >= 1 && res <= 4) {
    currentConfig.resolution = res;
    if (selectedPattern < MAX_PATTERNS) {
      patterns[selectedPattern] = currentConfig;
      patterns[selectedPattern].active = true;
    }
    // Notifica mudança de resolution para atualizar cache de durações de nota
    if (onTrackChanged) onTrackChanged();
  }
}

void EuclideanSequencer::setNoteLength(uint16_t length) {
  // Validar range: 50-700ms
  if (length >= 50 && length <= 700) {
    currentConfig.noteLength = length;
    if (selectedPattern < MAX_PATTERNS) {
      patterns[selectedPattern] = currentConfig;
      patterns[selectedPattern].active = true;
    }
  }
}

bool EuclideanSequencer::getPatternBit(uint8_t step) const {
  if (step < pattern.size()) {
    return pattern[step];
  }
  return false;
}

void EuclideanSequencer::savePattern(uint8_t slot) {
  if (slot < MAX_PATTERNS) {
    patterns[slot] = currentConfig;
    patterns[slot].active = true;
  }
}

void EuclideanSequencer::loadPattern(uint8_t slot) {
  if (slot < MAX_PATTERNS && patterns[slot].active) {
    currentConfig = patterns[slot];
    generatePattern();
  }
}

void EuclideanSequencer::clearPattern(uint8_t slot) {
  if (slot < MAX_PATTERNS) {
    patterns[slot].active = false;
  }
}

// Track helpers
void EuclideanSequencer::setSelectedPattern(uint8_t idx) {
  selectedPattern = idx % 8;
  if (selectedPattern < MAX_PATTERNS) {
    // Carregar padrão salvo ou criar novo se não existir
    if (patterns[selectedPattern].active) {
      // Padrão já existe - carregar valores salvos
      currentConfig = patterns[selectedPattern];
    } else {
      // Padrão novo - criar com defaults
      currentConfig.steps = DEFAULT_STEPS;
      currentConfig.hits = 4;
      currentConfig.offset = 0;
      currentConfig.note = 36 + selectedPattern;  // base 12 for patterns
      currentConfig.velocity = 100;
      currentConfig.midiChannel = 1;
      currentConfig.resolution = 2;
      currentConfig.noteLength = 100;  // Duração padrão: 100ms
      currentConfig.playMode = PLAY;
      currentConfig.active = true;
      
      // Guardar no slot da track
      patterns[selectedPattern] = currentConfig;
    }
    
    generatePattern();
    
    // Notifica mudança de track/resolution para atualizar cache
    if (onTrackChanged) onTrackChanged();
  }
}
uint8_t EuclideanSequencer::getSelectedPattern() const {
  return selectedPattern;
}
uint8_t EuclideanSequencer::getSelectedTrackNumber() const {
  return selectedPattern + 1; // 1..8
}

void EuclideanSequencer::loadPatternConfig() {
  // Carrega os parâmetros da track selecionada em currentConfig
  if (selectedPattern < MAX_PATTERNS && patterns[selectedPattern].active) {
    currentConfig = patterns[selectedPattern];
    generatePattern();
  }
}

void EuclideanSequencer::nextEditParam() {
  // Parâmetros em lista única: PLAY, STEPS, HITS, OFFSET, NOTE, VELOCITY, MIDI_CHANNEL, RESOLUTION, TRACK, TEMPO, CLOCK_IO, NOTE_OUT, MIDI_MAP, OSC_MAP
  // Definir o primeiro e o último parâmetro editável
  constexpr EditParam FIRST_PARAM = PARAM_PLAY;
  constexpr EditParam LAST_PARAM = PARAM_TEMPO;
  if (currentEditParam == LAST_PARAM) {
    currentEditParam = FIRST_PARAM;
  } else {
    currentEditParam = (EditParam)(currentEditParam + 1);
    if (currentEditParam > LAST_PARAM) currentEditParam = FIRST_PARAM;
  }
}

void EuclideanSequencer::prevEditParam() {
  constexpr EditParam FIRST_PARAM = PARAM_PLAY;
  constexpr EditParam LAST_PARAM = PARAM_TEMPO;
  if (currentEditParam == FIRST_PARAM) {
    currentEditParam = LAST_PARAM;
  } else {
    currentEditParam = (EditParam)(currentEditParam - 1);
    if (currentEditParam < FIRST_PARAM) currentEditParam = LAST_PARAM;
  }
  // nenhuma ação adicional necessária ao mudar param
}

// nextGroup / prevGroup: compatibilidade com mapeamentos MIDI/OSC que
// pedem alternar grupos de edição. Implementados como aliases para
// nextEditParam()/prevEditParam() para manter comportamento esperado.
void EuclideanSequencer::nextGroup() {
  nextEditParam();
}

void EuclideanSequencer::prevGroup() {
  prevEditParam();
}


void EuclideanSequencer::incrementParam(int8_t amount) {
  switch (currentEditParam) {
    case PARAM_PLAY:
      // Cicla entre PLAY e STOP
      {
        int nextMode = (int)currentConfig.playMode + amount;
        nextMode = ((nextMode % 2) + 2) % 2;  // Garante valores 0-1 mesmo com negative
        currentConfig.playMode = (PlayMode)nextMode;
        if (selectedPattern < MAX_PATTERNS) {
          patterns[selectedPattern].playMode = currentConfig.playMode;
        }
        if (currentConfig.playMode == PLAY) {
          start();  // Inicia quando muda para PLAY
        } else {
          stop();   // Para quando muda para STOP
        }
      }
      break;
    case PARAM_TRACK:
      setSelectedPattern(constrain((int)selectedPattern + amount, 0, 7));
      break;
    case PARAM_MIDI_CHANNEL:
      setMidiChannel(constrain(currentConfig.midiChannel + amount, 0, 15));
      break;
    case PARAM_NOTE:
      // Note: incrementos de 1 em 1
      setNote(constrain(currentConfig.note + amount, 0, 127));
      break;
    case PARAM_NOTE_LENGTH:
      // Note Length: incrementos de 50ms (50-700ms)
      setNoteLength(constrain((int)currentConfig.noteLength + (amount * 50), 50, 700));
      break;
    case PARAM_VELOCITY:
      // Velocity: incrementos de 1 em 1 (0-100)
      setVelocity(constrain(currentConfig.velocity + amount, 0, 127));
      break;
    case PARAM_RESOLUTION:
      setResolution(constrain((int)currentConfig.resolution + amount, 1, 4));
      break;
    case PARAM_STEPS:
      setSteps(constrain((int)currentConfig.steps + amount, 1, MAX_STEPS));
      break;
    case PARAM_HITS:
      // Hits: incrementos de 1
      {
        int next = constrain((int)currentConfig.hits + amount, 1, (int)MAX_HITS);
        setHits((uint8_t)next);
      }
      break;
    case PARAM_OFFSET: {
      // Ajuste para comportamento user-facing 1..N com wrap correto
      if (currentConfig.steps == 0) break;
      int user = (int)currentConfig.offset + 1; // converter intern 0..N-1 -> user 1..N
      user += amount; // amount pode ser negativo
      // wrap em 1..steps
      while (user < 1) user += currentConfig.steps;
      while (user > currentConfig.steps) user -= currentConfig.steps;
      setOffset((uint8_t)(user - 1)); // converter de volta para interno
      break;
    }
    case PARAM_TEMPO:
      // Tempo é global do MidiClock, será tratado no main.cpp
      break;
    
    case PARAM_DUB:
      setTrackEnabled(selectedPattern, !isTrackEnabled(selectedPattern));
      break;
  }
}

void EuclideanSequencer::decrementParam(int8_t amount) {
  incrementParam(-amount);
}



// Polyphony helpers
bool EuclideanSequencer::isTrackActive(uint8_t trackIdx) const {
  if (trackIdx < MAX_PATTERNS) {
    return patterns[trackIdx].active;
  }
  return false;
}

uint8_t EuclideanSequencer::getTrackNote(uint8_t trackIdx) const {
  if (trackIdx < MAX_PATTERNS && patterns[trackIdx].active) {
    return patterns[trackIdx].note;
  }
  return 0;
}


uint8_t EuclideanSequencer::getTrackVelocity(uint8_t trackIdx) const {
  if (trackIdx < MAX_PATTERNS && patterns[trackIdx].active) {
    return patterns[trackIdx].velocity;
  }
  return 100;  // velocidade padrão
}

uint8_t EuclideanSequencer::getTrackMidiChannel(uint8_t trackIdx) const {
  if (trackIdx < MAX_PATTERNS && patterns[trackIdx].active) {
    return patterns[trackIdx].midiChannel;
  }
  return 0;
}

uint8_t EuclideanSequencer::getTrackResolution(uint8_t trackIdx) const {
  if (trackIdx < MAX_PATTERNS && patterns[trackIdx].active) {
    return patterns[trackIdx].resolution;
  }
  return 1;
}

uint8_t EuclideanSequencer::getTrackSteps(uint8_t trackIdx) const {
  if (trackIdx < MAX_PATTERNS && patterns[trackIdx].active) {
    return patterns[trackIdx].steps;
  }
  return DEFAULT_STEPS;
}

uint16_t EuclideanSequencer::getTrackNoteLength(uint8_t trackIdx) const {
  if (trackIdx < MAX_PATTERNS && patterns[trackIdx].active) {
    return patterns[trackIdx].noteLength;
  }
  return 100;  // default 100ms
}

uint8_t EuclideanSequencer::getTrackHits(uint8_t trackIdx) const {
  if (trackIdx < MAX_PATTERNS && patterns[trackIdx].active) {
    return patterns[trackIdx].hits;
  }
  return 1;
}

uint8_t EuclideanSequencer::getTrackOffset(uint8_t trackIdx) const {
  if (trackIdx < MAX_PATTERNS && patterns[trackIdx].active) {
    return patterns[trackIdx].offset;
  }
  return 0;
}

bool EuclideanSequencer::getTrackPatternBit(uint8_t trackIdx, uint8_t step) const {
  if (trackIdx < MAX_PATTERNS && patterns[trackIdx].active) {
    // Regenerar o padrão para esse track e verificar o bit
    // Para simplificar, assumimos que o padrão foi calculado ao salvar
    // Aqui apenas retornamos se o bit estaria ligado
    if (step < patterns[trackIdx].steps) {
      // Usa a mesma lógica de Bjorklund
      uint8_t steps = patterns[trackIdx].steps;
      uint8_t hits = patterns[trackIdx].hits;
      if (hits > steps) hits = steps;
      
      // Aplicar offset como rotação à direita e avaliar regra do módulo
      if (steps == 0 || hits == 0) return false;
      if (hits == steps) return true;

      uint8_t src = step;
      if (patterns[trackIdx].offset % steps != 0) {
        uint8_t off = patterns[trackIdx].offset % steps;
        src = (step + steps - off) % steps;
      }
      return ((((uint16_t)src * hits) % steps) < hits);
    }
  }
  return false;
}

// NOTE: placeholder edit-mode functions removed — edit-mode handled in main application logic

void EuclideanSequencer::receiveMidiClock(uint8_t inIndex) {
  // Chamado quando o sequenciador recebe clock MIDI de uma porta específica
  // inIndex: 0=DIN1, 1=DIN2, 2=DIN3, 3=BLE, 4=USB
  // Nota: Atualmente não utilizado, pois o sequenciador usa seu próprio clock interno
  
  if (!isRunning) return;  // Apenas processa se está rodando
  
  // Clock MIDI não é mais utilizado para sincronização
  // O sequenciador usa seu próprio relógio interno baseado em millis()
}

// Novo: enable/disable por track
bool EuclideanSequencer::isTrackEnabled(uint8_t trackIdx) const {
  if (trackIdx < MAX_PATTERNS) {
    return patterns[trackIdx].enabled;
  }
  return true;
}

void EuclideanSequencer::setTrackEnabled(uint8_t trackIdx, bool enabled) {
  if (trackIdx < MAX_PATTERNS) {
    patterns[trackIdx].enabled = enabled;
    if (trackIdx == selectedPattern) {
      currentConfig.enabled = enabled;
    }
    if (onTrackChanged) onTrackChanged();
  }
}
