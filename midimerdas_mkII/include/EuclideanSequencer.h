#ifndef EUCLIDEAN_SEQUENCER_H
#define EUCLIDEAN_SEQUENCER_H

#include <Arduino.h>
#include <vector>

class EuclideanSequencer {
private:
  static const uint8_t MAX_STEPS = 16;      // máximo de passos na sequência
  static const uint8_t MAX_HITS = 16;       // máximo de hits (notas) na sequência (1..16)
  static const uint8_t MAX_PATTERNS = 8;    // máximo de padrões salvos
  static const uint8_t DEFAULT_STEPS = 16;  // 16 passos padrão (permite até 32)
  static const uint8_t MIDI_PPQN = 24;      // 24 pulsos por quarter note

public:
  // Modo de play
  enum PlayMode {
    PLAY = 0,             // Sequenciador tocando
    STOP = 1              // Sequenciador parado
  };

private:
  struct EuclideanPattern {
    uint8_t steps;                          // número total de passos (1-32)
    uint8_t hits;                           // número de hits a distribuir (0-steps)
    uint8_t offset;                         // rotação do padrão (0-steps)
    uint8_t note;                           // nota MIDI (0-127)
    uint8_t midiChannel;                    // canal MIDI (0-15)
    uint8_t resolution;                     // 1=1/4, 2=1/8, 3=1/16, 4=1/32
    uint8_t velocity;                       // velocidade MIDI (0-127, padrão 127)
    uint16_t noteLength;                    // duração da nota em ms (50-500, padrão 100)
    PlayMode playMode;                      // modo de sincronização da trilha
    bool active;                            // (antigo, pode ser mantido para compatibilidade)
    bool enabled;                           // NOVO: permite ligar/desligar a track individualmente
  };
public:
  // Novo: enable/disable por track
  bool isTrackEnabled(uint8_t trackIdx) const;
  void setTrackEnabled(uint8_t trackIdx, bool enabled);

  std::vector<bool> pattern;                // padrão euclidiano atual (true = hit, false = rest)
  EuclideanPattern currentConfig;
  EuclideanPattern patterns[MAX_PATTERNS];  // padrões salvos
  
  uint8_t currentStep;                      // posição atual na sequência
  uint8_t selectedPattern;                  // índice do padrão selecionado
  uint8_t trackCurrentStep[8];              // passo atual de cada track para UI
  
  bool isRunning;
  unsigned long lastStepTime;
  

public:
  // Parâmetros de edição do modo sequenciador (ordem de navegação por click)
  enum EditParam {
    // GRUPO MAIN (0-9): parâmetros principais em nova ordem
    PARAM_PLAY = 0,
    PARAM_TRACK = 1,
    PARAM_DUB = 2,              // Novo: enable/disable por track
    PARAM_MIDI_CHANNEL = 3,    // Movido para aqui (antes era 100)
    PARAM_NOTE = 4,
    PARAM_NOTE_LENGTH = 5,     // Duração da nota (50-700ms) - logo após NOTE
    PARAM_VELOCITY = 6,
    PARAM_RESOLUTION = 7,
    PARAM_STEPS = 8,
    PARAM_HITS = 9,
    PARAM_OFFSET = 10,
    // GRUPO OUTPUT (100-104): parâmetros de saída (valores altos para evitar confusão)
    PARAM_TEMPO = 100
    , PARAM_CLOCK_IO = 101
    , PARAM_NOTE_OUT = 102
    , PARAM_MIDI_MAP = 103
    , PARAM_OSC_MAP = 104
  };
  
  // Seleção de protocolo de saída (USB, BLE, OSC, DIN)
  // OutputProtocol as bitmask to allow combinations (USB, BLE, OSC, DIN)
  enum OutputProtocol : uint8_t {
    OUT_NONE = 0x00,
    OUT_USB = 0x01,
    OUT_BLE = 0x02,
    OUT_OSC = 0x04,
    OUT_DIN = 0x08,
    OUT_ALL = 0x0F
  };
  
public:

  // Função interna para gerar padrão euclidiano
  void generatePattern();
  
  // Algoritmo de Bjorklund para sequências euclidianas
  void bjorklundAlgorithm(uint8_t steps, uint8_t hits);
  
  EditParam currentEditParam;
  uint8_t currentGroup;  // 0 = MAIN group, 1 = OUTPUT group
  
  // Saídas separadas (antigo, mantido por compatibilidade)
  OutputProtocol outputNotes = OUT_DIN;
  OutputProtocol outputClock = OUT_DIN;
  // Novo: flags simples ON/OFF para feedback mapping (MIDI-only e OSC-only)
  bool outputMidiMap = true; // true = enviar feedback MIDI, false = suprimir
  bool outputOSCMap = true;  // true = enviar feedback OSC, false = suprimir
  
public:
  EuclideanSequencer();
  
  // Inicialização
  void begin();
  
  // Controle do sequenciador
  void start();
  void stop();
  void reset();
  
  // Configuração de padrão
  void setSteps(uint8_t steps);
  void setHits(uint8_t hits);
  void setOffset(uint8_t offset);
  void setNote(uint8_t note);
  void setVelocity(uint8_t vel);
  void setMidiChannel(uint8_t channel);
  void setResolution(uint8_t res);  // 1=1/4, 2=1/8, 3=1/16, 4=1/32
  void setNoteLength(uint16_t length);  // duração nota em ms (50-500)
  // Saídas
  void setOutputNotes(OutputProtocol out) { outputNotes = out; }
  void setOutputClock(OutputProtocol out) { outputClock = out; }
  void setOutputMidiMap(bool on) { outputMidiMap = on; }
  void setOutputOSCMap(bool on) { outputOSCMap = on; }
  
  // Leitura do estado
  bool isPatternRunning() const { return isRunning; }
  uint8_t getCurrentStep() const { return currentStep; }
  void setCurrentStep(uint8_t step) { currentStep = step % currentConfig.steps; }
  bool getPatternBit(uint8_t step) const;   // retorna o valor do bit em um passo específico
  
  // Gerenciamento de padrões
  void savePattern(uint8_t slot);
  void loadPattern(uint8_t slot);
  void clearPattern(uint8_t slot);
  // Seleção de Track/Padrão (0-7) e número de track (1-8)
  void setSelectedPattern(uint8_t idx);
  uint8_t getSelectedPattern() const;
  uint8_t getSelectedTrackNumber() const;
  
  // Carrega os parâmetros da track selecionada em currentConfig
  void loadPatternConfig();
  
  // Polyphony: obter dados de múltiplas tracks
  bool isTrackActive(uint8_t trackIdx) const;
  bool getTrackPatternBit(uint8_t trackIdx, uint8_t step) const;
  // Passo atual por track (para UI multi-agulhas)
  void setTrackCurrentStep(uint8_t trackIdx, uint8_t step) { if (trackIdx < 8) trackCurrentStep[trackIdx] = step; }
  uint8_t getTrackCurrentStep(uint8_t trackIdx) const { return (trackIdx < 8) ? trackCurrentStep[trackIdx] : 0; }
  // Getters para track config
  uint8_t getTrackNote(uint8_t trackIdx) const;
  uint8_t getTrackVelocity(uint8_t trackIdx) const;
  uint8_t getTrackMidiChannel(uint8_t trackIdx) const;
  uint8_t getTrackResolution(uint8_t trackIdx) const;
  uint8_t getTrackSteps(uint8_t trackIdx) const;
  uint16_t getTrackNoteLength(uint8_t trackIdx) const;
  // Novos getters para preservação de presets
  uint8_t getTrackHits(uint8_t trackIdx) const;
  uint8_t getTrackOffset(uint8_t trackIdx) const;
  
  // Navegação de edição no sequenciador
  void nextEditParam();   // navega para próximo param dentro do grupo
  void prevEditParam();   // navega para param anterior dentro do grupo
  void nextGroup();       // alterna para próximo grupo
  void prevGroup();       // alterna para grupo anterior
  EditParam getCurrentEditParam() const { return currentEditParam; }
  uint8_t getCurrentGroup() const { return currentGroup; }
  bool isRunningState() const { return isRunning; }
  
  // Controle de modo de edição
  // deprecated: toggle/set param edit mode were placeholders and removed
  
  void incrementParam(int8_t amount);       // incrementa o parâmetro selecionado
  void decrementParam(int8_t amount);       // decrementa o parâmetro selecionado
  
  // Leitura de parâmetros atuais
  uint8_t getSteps() const { return currentConfig.steps; }
  uint8_t getHits() const { return currentConfig.hits; }
  uint8_t getOffset() const { return currentConfig.offset; }
  uint8_t getNote() const { return currentConfig.note; }
  uint8_t getVelocity() const { return currentConfig.velocity; }
  uint8_t getMidiChannel() const { return currentConfig.midiChannel; }
  uint8_t getResolution() const { return currentConfig.resolution; }
  uint16_t getNoteLength() const { return currentConfig.noteLength; }
  OutputProtocol getOutputNotes() const { return outputNotes; }
  OutputProtocol getOutputClock() const { return outputClock; }
  bool getOutputMidiMap() const { return outputMidiMap; }
  bool getOutputOSCMap() const { return outputOSCMap; }
  
  // Play mode (sincronização)
  PlayMode getPlayMode() const { return currentConfig.playMode; }
  void setPlayMode(PlayMode mode) { currentConfig.playMode = mode; }
  
  const char* getPlayModeName(PlayMode mode) const {
    switch (mode) {
      case PLAY: return "Play";
      case STOP: return "Stop";
      default: return "---";
    }
  }
  
  // Recebe clock MIDI de um porto específico (chamado por MidiClock quando recebe sync)
  void receiveMidiClock(uint8_t inIndex);
  
  // Preset management
  // Preset functionality removed: selection and persistence handled in-memory only
  
  // Callbacks para integração com engine MIDI
  void (*onSendMidi)(uint8_t status, uint8_t data1, uint8_t data2) = nullptr;
  void (*onTrackChanged)() = nullptr;  // Chamado quando uma track é ativada ou modificada
};

#endif
