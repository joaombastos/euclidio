#ifndef MIDI_CC_MAPPING_H
#define MIDI_CC_MAPPING_H

#include <stdint.h>

class EuclideanSequencer;
class MidiClock;

// Mapeamento de MIDI CC para parâmetros do sequenciador
// Canal MIDI: 10 (índice 9, já que MIDI usa 1-16)
class MidiCCMapping {
public:
	// Callback para processar comandos MIDI CC
	static void processCC(uint8_t channel, uint8_t cc, uint8_t value, EuclideanSequencer* seq, MidiClock* clock = nullptr);
	
	// Callback para processar comandos MIDI Note (encoder clicks)
	static void processNote(uint8_t channel, uint8_t note, uint8_t velocity, bool isNoteOn, EuclideanSequencer* seq);
	
	// Enviar feedback MIDI CC dos estados atuais
	static void sendFeedback(EuclideanSequencer* seq, MidiClock* clock = nullptr);
	static void sendFeedbackSteps(EuclideanSequencer* seq);
	static void sendFeedbackHits(EuclideanSequencer* seq);
	static void sendFeedbackOffset(EuclideanSequencer* seq);
	static void sendFeedbackNote(EuclideanSequencer* seq);
	static void sendFeedbackVelocity(EuclideanSequencer* seq);
	static void sendFeedbackChannel(EuclideanSequencer* seq);
	static void sendFeedbackResolution(EuclideanSequencer* seq);
	static void sendFeedbackTrack(EuclideanSequencer* seq);
	static void sendFeedbackPlay(EuclideanSequencer* seq, MidiClock* clock = nullptr);
	static void sendFeedbackTempo(MidiClock* clock);
	
	// Getters para os valores de CC mapeados
	static uint8_t getCCSteps() { return CC_STEPS; }
	static uint8_t getCCHits() { return CC_HITS; }
	static uint8_t getCCOffset() { return CC_OFFSET; }
	static uint8_t getCCNote() { return CC_NOTE; }
	static uint8_t getCCVelocity() { return CC_VELOCITY; }
	static uint8_t getCCChannel() { return CC_CHANNEL; }
	static uint8_t getCCResolution() { return CC_RESOLUTION; }
	static uint8_t getCCTrack() { return CC_TRACK; }

	static uint8_t getCCTempo() { return CC_TEMPO; }
	static uint8_t getCCNoteLength() { return CC_NOTE_LENGTH; }
	static uint8_t getNoteEncoderDoubleClick() { return NOTE_ENCODER_DOUBLE_CLICK; }
	static uint8_t getNoteEncoderLongPress() { return NOTE_ENCODER_LONG_PRESS; }

	// Canal MIDI de controle (10 = índice 9)
	static const uint8_t MIDI_CONTROL_CHANNEL = 9;  // 0-15 (canal 1-16)
	
private:
	// Nota MIDI base para alternar Dub por track (8 notas, uma por track)
	static const uint8_t NOTE_TOGGLE_DUB_BASE = 76; // notas 76..83 correspondem às tracks 0..7
	// Notas MIDI para controles de transporte e grupo
	static const uint8_t NOTE_TOGGLE_PLAY_ALT = 0; // Note On 0 - alternativa para Play/Stop universal
	static const uint8_t NOTE_TOGGLE_GROUP = 78; // F#5 - Avança/retrocede grupo (velocity >=64 -> próximo)
	// Notas para controles do sequenciador harmônico
	static const uint8_t NOTE_HARMONIC_TOGGLE = 90; // Alterna modo Harmônico (On/Off)
	static const uint8_t NOTE_HARMONIC_CHORD_EDIT = 91; // Entrar/Sair modo edição de acordes
	static const uint8_t NOTE_HARMONIC_SLOT_BASE = 92; // 92..99 toggles para slots de acordes 0..7
	// Mapeamento de CC para parâmetros do sequenciador
	static const uint8_t CC_VELOCITY = 19;
	static const uint8_t CC_STEPS = 20;
	static const uint8_t CC_HITS = 21;
	static const uint8_t CC_OFFSET = 22;
	static const uint8_t CC_NOTE = 23;
	static const uint8_t CC_CHANNEL = 24;
	static const uint8_t CC_RESOLUTION = 25;
	static const uint8_t CC_TRACK = 26;
	static const uint8_t CC_TEMPO = 28;
	static const uint8_t CC_NOTE_LENGTH = 31;  // Duração da nota (50-700ms)
	// CCs para o sequenciador harmônico (evitam sobreposição com CCs já usados)
	static const uint8_t CC_HARM_TONIC = 40;
	static const uint8_t CC_HARM_SCALE = 41;
	static const uint8_t CC_HARM_MODE = 42;
	static const uint8_t CC_HARM_STEPS = 43;
	static const uint8_t CC_HARM_HITS = 44;
	static const uint8_t CC_HARM_OFFSET = 45;
	static const uint8_t CC_HARM_POLY = 46;
	static const uint8_t CC_HARM_VELOCITY = 47;
	static const uint8_t CC_HARM_NOTE_LENGTH = 48; // 10-2000ms
	// Ajuste de oitava para sequenciador harmônico (-2..+2)
	static const uint8_t CC_HARM_OCTAVE = 49;
	// Resolução do sequenciador harmônico (0=1/4,1=1/8,2=1/16)
	static const uint8_t CC_HARM_RESOLUTION = 58;
	// CCs para edição de chord list
	static const uint8_t CC_HARM_CHORD_SELECT = 50;
	static const uint8_t CC_HARM_CHORD_SET_DEGREE = 51;
	static const uint8_t CC_HARM_CHORD_INSERT = 52;
	static const uint8_t CC_HARM_CHORD_DELETE = 53;
	// Grupo 2 (parâmetros compartilhados)
	// (Group2 removed: CC 54..56 deprecated)

	static const uint8_t NOTE_ENCODER_DOUBLE_CLICK = 49;  // Duplo click (muda grupo)
	static const uint8_t NOTE_ENCODER_LONG_PRESS = 50;    // Long press (muda modo: routing→load→sequencer→save)
	
	// Routing matrix note mapping removed — notas 51..65 não são mais usadas para controle via MIDI
	
	// Funções auxiliares de conversão
	static int mapCCToSteps(uint8_t value);
	static int mapCCToHits(uint8_t value);
	static int mapCCToOffset(uint8_t value);
	static int mapCCToNote(uint8_t value);
	static int mapCCToVelocity(uint8_t value);
	static int mapCCToChannel(uint8_t value);
	static int mapCCToResolution(uint8_t value);
	static int mapCCToTrack(uint8_t value);
	static int mapCCToTempo(uint8_t value);
	static int mapCCToNoteLength(uint8_t value);
	// Harmonic helpers
	static uint8_t mapCCToTonic(uint8_t value);
	static int mapCCToScaleIndex(uint8_t value);
	static uint8_t mapCCToPolyphony(uint8_t value);
	static int mapCCToLargeNoteLength(uint8_t value);
	static uint8_t mapCCToDegree(uint8_t value);
};

#endif // MIDI_CC_MAPPING_H
