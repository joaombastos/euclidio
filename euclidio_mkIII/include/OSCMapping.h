#ifndef OSC_MAPPING_H
#define OSC_MAPPING_H

#include <stdint.h>

class EuclideanSequencer;
class MidiClock;
class EuclideanHarmonicSequencer;

// Mapeamento de OSC para parâmetros do sequenciador
class OSCMapping {
public:
	// Processa mensagem OSC e controla o sequenciador
	static void processMessage(const char* path, int argc, float* argv, EuclideanSequencer* seq, MidiClock* clock = nullptr);
	
	// Envia feedback OSC dos estados atuais
	static void sendAllFeedback(EuclideanSequencer* seq, MidiClock* clock = nullptr);
	// Harmonic sequencer OSC feedback
	static void sendAllHarmonicFeedback(class EuclideanHarmonicSequencer* hseq, MidiClock* clock = nullptr);
	static void sendAllHarmonicFeedbackForced(class EuclideanHarmonicSequencer* hseq, MidiClock* clock = nullptr);
	// Envia todos os OSC de uma vez (ignora cache)
	static void sendAllFeedbackForced(EuclideanSequencer* seq, MidiClock* clock = nullptr);
	static void setOSCController(class OSCController* controller) { oscController = controller; }
	
	// Paths OSC para controle do sequenciador
	
	static const char* PATH_STEPS;
	static const char* PATH_HITS;
	static const char* PATH_OFFSET;
	static const char* PATH_NOTE;
	static const char* PATH_VELOCITY;
	static const char* PATH_CHANNEL;
	static const char* PATH_RESOLUTION;
	static const char* PATH_TRACK;
	
	static const char* PATH_PLAYSTOP;
	static const char* PATH_TEMPO;
	static const char* PATH_NOTE_LENGTH;
	// Base path for dub per-track mapping: clients should use /sequencer/dub/<n>
	static const char* PATH_DUB_BASE;

	// Harmonic sequencer OSC paths
	static const char* PATH_HARMONIC_BASE;
	static const char* PATH_HARMONIC_TONALITY;
	static const char* PATH_HARMONIC_SCALE;
	static const char* PATH_HARMONIC_SCALE_NAME;
	static const char* PATH_HARMONIC_MODE;
	static const char* PATH_HARMONIC_STEPS;
	static const char* PATH_HARMONIC_HITS;
	static const char* PATH_HARMONIC_OFFSET;
	static const char* PATH_HARMONIC_POLY;
	static const char* PATH_HARMONIC_VELOCITY;
	static const char* PATH_HARMONIC_NOTE_LENGTH;
	static const char* PATH_HARMONIC_OCTAVE;
	static const char* PATH_HARMONIC_ACTIVE;
	static const char* PATH_HARMONIC_TRACK;
	static const char* PATH_HARMONIC_RESOLUTION;
	static const char* PATH_HARMONIC_CHANNEL;
	static const char* PATH_HARMONIC_CHORDS_COUNT;
	// Chord list OSC paths
	static const char* PATH_HARMONIC_CHORDS_SELECT;
	static const char* PATH_HARMONIC_CHORDS_SET;
	static const char* PATH_HARMONIC_CHORDS_INSERT;
	static const char* PATH_HARMONIC_CHORDS_DELETE;
	static const char* PATH_HARMONIC_CHORDS_TOGGLE;
	
	// Paths para encoder (apenas double-click e long-press via OSC)
	static const char* PATH_ENCODER_DOUBLE_CLICK;
	static const char* PATH_ENCODER_LONG_PRESS;

	
	// Paths para matriz de roteamento
	static const char* PATH_ROUTING_TOGGLE;
	

private:
	static class OSCController* oscController;
	
	// Valores anteriores para detecção de mudanças
	static uint8_t lastSteps;
	static uint8_t lastHits;
	static uint8_t lastOffset;
	static uint8_t lastNote;
	static uint8_t lastVelocity;
	static uint8_t lastChannel;
	static uint8_t lastResolution;
	static uint8_t lastTrack;
	static bool lastPlayState;
	static float lastTempo;
	static bool lastDubState[8];
	static uint16_t lastNoteLength;

	// Harmonic sequencer last-value caches (to send per-parameter feedback)
	static int32_t lastHarmonicTonality;
	static int32_t lastHarmonicScale;
	static int32_t lastHarmonicMode;
	static int32_t lastHarmonicSteps;
	static int32_t lastHarmonicHits;
	static int32_t lastHarmonicOffset;
	static int32_t lastHarmonicPoly;
	static int32_t lastHarmonicVelocity;
	static int32_t lastHarmonicNoteLength;
	static int32_t lastHarmonicActive;
	static int32_t lastHarmonicChordSlot[8];
	static int32_t lastHarmonicTrackActive[8];
	static int32_t lastHarmonicOctave;

	// Additional caches
	static int32_t lastHarmonicTrack;
	static int32_t lastHarmonicResolution;
	static int32_t lastHarmonicChannel;
	static int32_t lastHarmonicChordCount;
	
	// Funções auxiliares
	static int mapFloatToInt(float value, int min, int max);
	static uint8_t mapFloatToUint8(float value, uint8_t min, uint8_t max);
	static void sendOSCIfChanged(const char* path, int32_t newValue, int32_t& lastValue);
	static void sendOSCIfChanged(const char* path, uint8_t newValue, uint8_t& lastValue);
	static void sendOSCIfChanged(const char* path, bool newValue, bool& lastValue);
	static void sendOSCIfChanged(const char* path, float newValue, float& lastValue);
	// Retorna nome textual de ScaleType (índice fornecido)
	static const char* scaleTypeToName(int scaleType);
};

#endif // OSC_MAPPING_H

