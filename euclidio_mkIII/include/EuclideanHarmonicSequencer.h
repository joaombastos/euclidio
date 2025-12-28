#ifndef EUCLIDEAN_HARMONIC_SEQUENCER_H
#define EUCLIDEAN_HARMONIC_SEQUENCER_H

#include <Arduino.h>
#include <vector>
#include <array>
#include <cstdint>
#include <cstring>
#include "EuclideanSequencer.h"

class EuclideanMidiEngine;
class MidiClock;

class EuclideanHarmonicSequencer {
public:
  // Getters públicos para UI multi-track
  bool isTrackEnabled(uint8_t t) const { return (t < MAX_TRACKS) ? enabled[t] : false; }
  bool isTrackActive(uint8_t t) const { return (t < MAX_TRACKS) ? uiActive[t] : false; }
  uint8_t getStepsForTrack(uint8_t t) const { return (t < MAX_TRACKS) ? steps[t] : 1; }
  uint8_t getChordListPosForTrack(uint8_t t) const { return (t < MAX_TRACKS) ? chordListPos[t] : 0; }
  uint8_t getCurrentStepForTrack(uint8_t t) const { return (t < MAX_TRACKS) ? currentStepPerTrack[t] : 0; }
  enum DistributionMode { DIST_CHORDS = 0, DIST_NOTES = 1 };
  EuclideanHarmonicSequencer();
  void begin(EuclideanMidiEngine* engine, MidiClock* clock);
  void start();
  void stop();
  void reset();
  // Reset all per-track parameters to defaults (stops playback)
  void resetToDefaults();
  void update(); // deve ser chamada no loop()

  // Parâmetros simples (operam sobre a track ativa)
  void setSteps(uint8_t steps);
  void setHits(uint8_t hits);
  void setOffset(uint8_t offset);
  void setTonic(uint8_t tonic); // 0-11
  void setScaleMajor(bool major); // true = major, false = natural minor
  void setBaseOctave(int8_t octave); // deslocamento de oitava para notas geradas (-2..+2). 0 -> base C3 (MIDI 48)
  void setPolyphony(uint8_t voices);
  static const uint8_t MAX_POLYPHONY = 5;
  void setResolutionIndex(uint8_t idx); // 0:1/4, 1:1/8, 2:1/16
  uint8_t getResolutionIndex() const;

  // Getters (return values for the active track)
  uint8_t getSteps() const { return steps[activeTrack]; }
  uint8_t getHits() const { return hits[activeTrack]; }
  uint8_t getOffset() const { return offset[activeTrack]; }
  bool isRunning() const { return running; }
  uint8_t getTonic() const { return tonic[activeTrack]; }
  bool isMajorScale() const { return majorScale[activeTrack]; }
  uint8_t getPolyphony() const { return polyphony[activeTrack]; }
  int8_t getBaseOctave() const { return baseOctave[activeTrack]; }
  bool getPatternBit(uint8_t idx) const { return (idx < patternLen[activeTrack]) ? pattern[activeTrack][idx] : false; }
  uint8_t getCurrentStep() const { return currentStep; }

  // Public accessors for UI and main (operate on active track)
  void setMidiChannel(uint8_t ch) { midiChannel[activeTrack] = ch & 0x0F; }
  uint8_t getMidiChannel() const { return midiChannel[activeTrack]; }
  void setVelocity(uint8_t v) { velocity[activeTrack] = constrain(v, (uint8_t)0, (uint8_t)127); }
  uint8_t getVelocity() const { return velocity[activeTrack]; }
  void setNoteLength(uint16_t ms) { noteLength[activeTrack] = ms; }
  uint16_t getNoteLength() const { return noteLength[activeTrack]; }
  void setDistributionMode(int m) { distributionMode[activeTrack] = (DistributionMode)m; }
  int getDistributionMode() const { return (int)distributionMode[activeTrack]; }
  // UI-visible Active flag (separate from internal playback `enabled`)
  void setActive(bool a);
  bool isActive() const { return uiActive[activeTrack]; }

  // Chord list management exposed
  void addChordDegree(uint8_t degree); // 0..6
  void removeLastChord();
  uint8_t getChordListSize() const { return chordListSize[activeTrack]; }
  uint8_t getChordListItem(uint8_t idx) const { return (idx < chordListSize[activeTrack]) ? chordList[activeTrack][idx] : 0; }
  uint8_t getChordListPos() const { return chordListPos[activeTrack]; }
  // Track handling (1..MAX_TRACKS for user-facing API)
  static const uint8_t MAX_TRACKS = 8;
  void setActiveTrack(uint8_t trackOneBased);
  uint8_t getActiveTrackNumber() const; // returns 1..MAX_TRACKS
  uint8_t scaleDegreeToMidi(uint8_t degree, int8_t octaveShift = 0) const;
  // Provide human-readable chord name for UI (e.g., Cmaj7, Dm7)
  void getChordName(uint8_t chordIndex, char* out, size_t len) const;
  // Format chord name including extensions up to `poly` voices (for UI display)
  void formatChordName(uint8_t chordIndex, uint8_t poly, char* out, size_t len) const;
  // Scale types supported
  enum ScaleType {
    SCALE_MAJOR,
    SCALE_NAT_MINOR,
    SCALE_HARM_MINOR,
    SCALE_MELODIC_MINOR,
    SCALE_IONIAN,
    SCALE_DORIAN,
    SCALE_PHRYGIAN,
    SCALE_LYDIAN,
    SCALE_MIXOLYDIAN,
    SCALE_AEOLIAN,
    SCALE_LOCRIAN,
    SCALE_MIX_B9_B13,
    SCALE_LYDIAN_SHARP9,
    SCALE_PHRYGIAN_DOMINANT,
    SCALE_LYDIAN_SHARP5,
    SCALE_HALF_WHOLE,
    SCALE_WHOLE_HALF,
    SCALE_AUGMENTED,
    SCALE_ALTERED,
    SCALE_PENT_MAJOR,
    SCALE_PENT_MINOR,
    SCALE_BLUES,
    SCALE_GYPSY,
    SCALE_JAPANESE,
    SCALE_HIRAJOSHI,
    SCALE_KUMOI,
    SCALE_ARABIC,
    SCALE_COUNT
  };
  void setScaleType(ScaleType t);
  ScaleType getScaleType() const { return (ScaleType)scaleType[activeTrack]; }
  // Return allowed scale degrees for catalog/filtering (0..6 typically)
  void getAllowedDegrees(std::vector<uint8_t> &out) const;
  // Advanced editing
  void setChordListItem(uint8_t idx, uint8_t degree);
  void insertChordAt(uint8_t idx, uint8_t degree);
  void removeChordAt(uint8_t idx);
  void moveChord(uint8_t idx, int8_t dir); // dir = -1 (left) or +1 (right)

  // Output mapping controls (MIDI / OSC feedback enable)
  void setOutputMidiMap(bool on) { outputMidiMap = on; }
  void setOutputOSCMap(bool on) { outputOSCMap = on; }
  bool getOutputMidiMap() const { return outputMidiMap; }
  bool getOutputOSCMap() const { return outputOSCMap; }


private:
  // Config (per-track where applicable)
  std::array<uint8_t, MAX_TRACKS> steps;
  std::array<uint8_t, MAX_TRACKS> hits;
  std::array<uint8_t, MAX_TRACKS> offset;
  std::array<uint8_t, MAX_TRACKS> tonic; // 0..11
  std::array<bool, MAX_TRACKS> majorScale;
  std::array<int8_t, MAX_TRACKS> baseOctave; // -2..+2
  std::array<uint8_t, MAX_TRACKS> polyphony; // até MAX_POLYPHONY
  // Additional musical params (per-track)
  std::array<uint8_t, MAX_TRACKS> midiChannel; // 0-15
  std::array<uint8_t, MAX_TRACKS> velocity; // 0-127
  std::array<uint16_t, MAX_TRACKS> noteLength; // ms
  std::array<DistributionMode, MAX_TRACKS> distributionMode;
  std::array<int, MAX_TRACKS> scaleType; // current ScaleType (stored as int)
  std::array<uint8_t, MAX_TRACKS> resolutionIndex; // 0:1/4, 1:1/8, 2:1/16

  // Runtime
  bool running;
  uint8_t currentStep;
  uint8_t lastStep;
  std::array<uint8_t, MAX_TRACKS> currentStepPerTrack; // per-track current step for UI
  std::array<bool, MAX_TRACKS> enabled; // per-track enabled/disabled
  std::array<bool, MAX_TRACKS> uiActive; // per-track UI Active flag (starts false)
  // Per-track patterns and chord lists (static buffers to avoid dynamic allocs)
  static const uint8_t MAX_STEPS = 32;
  static const uint8_t MAX_CHORDS = 16;
  bool pattern[MAX_TRACKS][MAX_STEPS];
  uint8_t patternLen[MAX_TRACKS];
  uint8_t activeTrack = 0; // internal 0-based active track index
  // Content: list of scale degrees to be used as chords (0..6) per track
  uint8_t chordList[MAX_TRACKS][MAX_CHORDS];
  uint8_t chordListSize[MAX_TRACKS];
  std::array<uint8_t, MAX_TRACKS> chordListPos; // position/index into chordList for next hit per track

  // Pending note-offs gerenciados localmente (note + channel + time)
  struct PendingOff { uint8_t note; uint8_t channel; uint32_t time; };
  static const uint8_t MAX_PENDING_OFFS = 64;
  PendingOff pendingOffs[MAX_PENDING_OFFS];
  uint8_t pendingOffsCount;

  // Request deferred forced feedback to be sent from `update()` context
  bool pendingFeedback;

  // Dependências
  EuclideanMidiEngine* engine;
  MidiClock* midiClock;

  // Output mapping toggles for feedback (global for harmonic sequencer)
  bool outputMidiMap = true;
  bool outputOSCMap = true;

  // Helpers
  void generatePattern();
  void generatePatternForTrack(uint8_t t);
  // Allocation-free bjorklund variant: fills a preallocated buffer `out` with 0/1 values.
  void bjorklundAlgorithm(std::vector<bool> &outPattern, uint8_t steps, uint8_t hits, uint8_t off);
  void bjorklundStatic(bool *out, uint8_t steps, uint8_t hits, uint8_t off, uint8_t max_len);
  void triggerChord(uint8_t degreeIndex);
  // Deferred pattern generation to avoid blocking during rapid encoder edits
  static const unsigned long PATTERN_DEBOUNCE_MS = 120;
  // flags used internally to defer pattern regeneration
  bool patternDirty[MAX_TRACKS];
  unsigned long patternLastEditTime[MAX_TRACKS];

  // Persistence removed: sequencer operates in RAM only
  // Chord list management (internal)
  void fillChordListFromScale();
  // Persistence helpers
  // (none additional)
};

#endif
