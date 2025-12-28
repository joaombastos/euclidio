#include "EuclideanHarmonicSequencer.h"
#include "EuclideanMidiEngine.h"
#include "MidiClock.h"
#include <algorithm>
// Feedback helpers
#include "MidiFeedback.h"
#include "OSCMapping.h"

// Intervals for scales (degrees in semitones from tonic)
// Many scale definitions (semitone offsets from tonic)
static const int MAJOR_SCALE[] = {0,2,4,5,7,9,11};
static const int MINOR_NATURAL_SCALE[] = {0,2,3,5,7,8,10};
static const int HARMONIC_MINOR_SCALE[] = {0,2,3,5,7,8,11};
static const int MELODIC_MINOR_SCALE[] = {0,2,3,5,7,9,11};
static const int PHRYGIAN_DOMINANT_SCALE[] = {0,1,4,5,7,8,10};
static const int MIX_B9_B13_SCALE[] = {0,1,4,5,6,8,10};
static const int LYDIAN_SHARP9_SCALE[] = {0,2,3,6,7,9,11};
static const int LYDIAN_SHARP5_SCALE[] = {0,2,4,6,8,9,11};
static const int AUGMENTED_SCALE[] = {0,1,4,5,8,9};
static const int WHOLE_HALF_SCALE[] = {0,2,3,5,6,8,9,11};
static const int HALF_WHOLE_SCALE[] = {0,1,3,4,6,7,9,10};
static const int ALTERED_SCALE[] = {0,1,3,4,6,8,10};
static const int PENT_MAJOR[] = {0,2,4,7,9};
static const int PENT_MINOR[] = {0,3,5,7,10};
static const int BLUES_SCALE[] = {0,3,5,6,7,10};
// Some exotic approximations
static const int GYPSY_SCALE[] = {0,1,4,5,7,8,10};
static const int JAPANESE_SCALE[] = {0,2,3,7,8};
static const int HIRAJOSHI_SCALE[] = {0,2,3,7,8};
static const int KUMOI_SCALE[] = {0,2,3,7,9};
static const int ARABIC_SCALE[] = {0,1,4,5,6,8,11};

static const uint8_t SCALE_LEN = 7; // default diatonic len

// Map scale enum to arrays and lengths
struct ScaleDef { const int* arr; uint8_t len; };

static const ScaleDef SCALE_DEFS[] = {
  { MAJOR_SCALE, 7 },
  { MINOR_NATURAL_SCALE, 7 },
  { HARMONIC_MINOR_SCALE, 7 },
  { MELODIC_MINOR_SCALE, 7 },
  { MAJOR_SCALE, 7 }, // IONIAN
  { MAJOR_SCALE, 7 }, // DORIAN (same intervals but interpreted differently)
  { PHRYGIAN_DOMINANT_SCALE, 7 },
  { LYDIAN_SHARP9_SCALE, 7 },
  { MAJOR_SCALE, 7 }, // MIXOLYDIAN
  { MINOR_NATURAL_SCALE, 7 }, // AEOLIAN
  { MINOR_NATURAL_SCALE, 7 }, // LOCRIAN
  { MIX_B9_B13_SCALE, 7 },
  { LYDIAN_SHARP9_SCALE, 7 },
  { PHRYGIAN_DOMINANT_SCALE, 7 },
  { LYDIAN_SHARP5_SCALE, 7 },
  { HALF_WHOLE_SCALE, 8 },
  { WHOLE_HALF_SCALE, 8 },
  { AUGMENTED_SCALE, 6 },
  { ALTERED_SCALE, 7 },
  { PENT_MAJOR, 5 },
  { PENT_MINOR, 5 },
  { BLUES_SCALE, 6 },
  { GYPSY_SCALE, 7 },
  { JAPANESE_SCALE, 5 },
  { HIRAJOSHI_SCALE, 5 },
  { KUMOI_SCALE, 5 },
  { ARABIC_SCALE, 7 }
};

// store selected scale type

EuclideanHarmonicSequencer::EuclideanHarmonicSequencer()
  : running(false), currentStep(0), lastStep(255), engine(nullptr), midiClock(nullptr), outputMidiMap(true), outputOSCMap(true) {
  // initialize defaults (delegated to resetToDefaults)
  // buffers are static-fixed; ensure counts cleared
  for (uint8_t t = 0; t < EuclideanHarmonicSequencer::MAX_TRACKS; ++t) {
    patternLen[t] = 0;
    chordListSize[t] = 0;
  }
  pendingOffsCount = 0;
  pendingFeedback = false;
  resetToDefaults();
}

void EuclideanHarmonicSequencer::resetToDefaults() {
  // initialize per-track arrays with defaults
  for (uint8_t t = 0; t < EuclideanHarmonicSequencer::MAX_TRACKS; ++t) {
    steps[t] = 16;
    hits[t] = 4;
    offset[t] = 0;
    tonic[t] = 0;
    majorScale[t] = true;
    baseOctave[t] = 0;
    polyphony[t] = 1;
    midiChannel[t] = 0;
    velocity[t] = 100;
    noteLength[t] = 200;
    distributionMode[t] = DIST_CHORDS;
    scaleType[t] = (int)EuclideanHarmonicSequencer::SCALE_MAJOR;
    resolutionIndex[t] = 1;
    chordListPos[t] = 0;
    // All tracks start OFF (not audible)
    enabled[t] = false;
    uiActive[t] = false;
    currentStepPerTrack[t] = 0;
    patternDirty[t] = false;
    patternLastEditTime[t] = 0;
    // ensure a minimal chord list exists for each track (start with a single degree = 1)
    chordListSize[t] = 1;
    chordList[t][0] = 0; // degree 0 -> root (C for tonic=0)
    chordListPos[t] = 0;
    activeTrack = t;
    generatePatternForTrack(t);
  }
  // default to track 1
  activeTrack = 0;
  // stop playback
  running = false;
  currentStep = 0;
  lastStep = 255;
  pendingOffsCount = 0;
}

// Track handling
void EuclideanHarmonicSequencer::setActiveTrack(uint8_t trackOneBased) {
  if (trackOneBased < 1) trackOneBased = 1;
  if (trackOneBased > EuclideanHarmonicSequencer::MAX_TRACKS) trackOneBased = EuclideanHarmonicSequencer::MAX_TRACKS;
  // store internally as 0-based index
  // add a private member 'activeTrack' if not present
  activeTrack = (uint8_t)(trackOneBased - 1);
  // schedule forced feedback to be sent from update() to avoid reentrancy/blocking
  pendingFeedback = true;
}

uint8_t EuclideanHarmonicSequencer::getActiveTrackNumber() const {
  return (uint8_t)(activeTrack + 1);
}

void EuclideanHarmonicSequencer::setResolutionIndex(uint8_t idx) {
  resolutionIndex[activeTrack] = idx % 3; // 3 supported values
}

uint8_t EuclideanHarmonicSequencer::getResolutionIndex() const {
  return resolutionIndex[activeTrack];
}

void EuclideanHarmonicSequencer::setScaleType(ScaleType t) {
  scaleType[activeTrack] = (int)t;
}

void EuclideanHarmonicSequencer::getAllowedDegrees(std::vector<uint8_t> &out) const {
  out.clear();
  // For diatonic-like scales, degrees 0..6
  const ScaleDef &sd = SCALE_DEFS[(int)scaleType[activeTrack] % (sizeof(SCALE_DEFS)/sizeof(SCALE_DEFS[0]))];
  if (sd.len >= 7) {
    for (uint8_t d = 0; d < 7; ++d) out.push_back(d);
  } else {
    // For pentatonic-like or shorter scales, map to available scale steps
    for (uint8_t i = 0; i < sd.len && i < 7; ++i) out.push_back(i);
  }
}

// simple getters already in header; ensure currentStep updated when generating pattern

void EuclideanHarmonicSequencer::begin(EuclideanMidiEngine* eng, MidiClock* clock) {
  engine = eng;
  midiClock = clock;
  pendingOffsCount = 0;
  // Persistence disabled: operate in RAM only (no loading from flash)
  // All tracks começam e permanecem em OFF (não audíveis)
  for (uint8_t t = 0; t < MAX_TRACKS; ++t) {
    enabled[t] = false;
    uiActive[t] = false;
  }
}

void EuclideanHarmonicSequencer::setActive(bool a) {
  uiActive[activeTrack] = a;
  if (a) {
    // enable playback for this track and start sequencer
    enabled[activeTrack] = true;
    // ensure global running state
    if (!running) start();
  } else {
    // disable playback for this track
    enabled[activeTrack] = false;
    // if no track remains enabled, stop sequencer
    bool any = false;
    for (uint8_t t = 0; t < MAX_TRACKS; ++t) { if (enabled[t]) { any = true; break; } }
    if (!any) stop();
  }
}

void EuclideanHarmonicSequencer::start() {
  running = true;
  // Inicia no passo 0 para garantir acorde no passo 1
  currentStep = 0;
  lastStep = 255;
  // Garante que o ciclo sempre começa no primeiro acorde da lista
  for (uint8_t t = 0; t < MAX_TRACKS; ++t) {
    chordListPos[t] = 0;
  }
}
void EuclideanHarmonicSequencer::stop() { running = false; }
void EuclideanHarmonicSequencer::reset() { currentStep = 0; lastStep = 255; }

void EuclideanHarmonicSequencer::setSteps(uint8_t s) {
  steps[activeTrack] = (s==0?1:s);
  patternDirty[activeTrack] = true;
  patternLastEditTime[activeTrack] = millis();
}
void EuclideanHarmonicSequencer::setHits(uint8_t h) {
  hits[activeTrack] = (h==0?1:h);
  patternDirty[activeTrack] = true;
  patternLastEditTime[activeTrack] = millis();
}
void EuclideanHarmonicSequencer::setOffset(uint8_t o) {
  offset[activeTrack] = o % steps[activeTrack];
  patternDirty[activeTrack] = true;
  patternLastEditTime[activeTrack] = millis();
}
void EuclideanHarmonicSequencer::setTonic(uint8_t t) { tonic[activeTrack] = t % 12; }
void EuclideanHarmonicSequencer::setScaleMajor(bool major) { majorScale[activeTrack] = major; }
void EuclideanHarmonicSequencer::setBaseOctave(int8_t oct) { baseOctave[activeTrack] = (int8_t)constrain((int)oct, -2, 2); }
void EuclideanHarmonicSequencer::setPolyphony(uint8_t voices) { polyphony[activeTrack] = constrain(voices, (uint8_t)1, (uint8_t)EuclideanHarmonicSequencer::MAX_POLYPHONY); }

void EuclideanHarmonicSequencer::generatePattern() {
  generatePatternForTrack(activeTrack);
}

void EuclideanHarmonicSequencer::generatePatternForTrack(uint8_t t) {
  if (t >= MAX_TRACKS) return;
  // generate into a fixed buffer to avoid allocations inside the Bjorklund algorithm
  uint8_t s = steps[t] == 0 ? 1 : steps[t];
  bool buf[EuclideanHarmonicSequencer::MAX_STEPS];
  // ensure buffer large enough
  if (s > EuclideanHarmonicSequencer::MAX_STEPS) s = EuclideanHarmonicSequencer::MAX_STEPS;
  bjorklundStatic(buf, s, hits[t], offset[t], EuclideanHarmonicSequencer::MAX_STEPS);
  // copy into static pattern buffer
  patternLen[t] = s;
  for (uint8_t i = 0; i < s; ++i) pattern[t][i] = buf[i];
  // clear remaining
  for (uint8_t i = s; i < EuclideanHarmonicSequencer::MAX_STEPS; ++i) pattern[t][i] = false;
}
void EuclideanHarmonicSequencer::bjorklundAlgorithm(std::vector<bool> &out, uint8_t s, uint8_t h, uint8_t off) {
  // Compatibility wrapper: uses allocation-free implementation internally
  if (s == 0) return;
  if (s > EuclideanHarmonicSequencer::MAX_STEPS) s = EuclideanHarmonicSequencer::MAX_STEPS;
  bool buf[EuclideanHarmonicSequencer::MAX_STEPS];
  bjorklundStatic(buf, s, h, off, EuclideanHarmonicSequencer::MAX_STEPS);
  out.clear();
  out.resize(s);
  for (uint8_t i = 0; i < s; ++i) out[i] = buf[i];
}

void EuclideanHarmonicSequencer::bjorklundStatic(bool *out, uint8_t s, uint8_t h, uint8_t off, uint8_t max_len) {
  if (!out || max_len == 0) return;
  if (s == 0) return;
  if (s > max_len) s = max_len;
  if (h > s) h = s;
  if (h == 0) {
    for (uint8_t i = 0; i < s; ++i) out[i] = false;
    return;
  }
  if (h == s) {
    for (uint8_t i = 0; i < s; ++i) out[i] = true;
    return;
  }
  // Simple, deterministic distribution: (i*h) mod s < h
  for (uint8_t i = 0; i < s; ++i) {
    out[i] = ((((uint16_t)i * h) % s) < h);
  }
  // rotation (offset)
  uint8_t roff = off % s;
  if (roff != 0) {
    // perform in-place rotation using a small temp buffer on stack
    bool temp[EuclideanHarmonicSequencer::MAX_STEPS];
    for (uint8_t i = 0; i < s; ++i) {
      uint8_t src = (i + s - roff) % s;
      temp[i] = out[src];
    }
    for (uint8_t i = 0; i < s; ++i) out[i] = temp[i];
  }
}

uint8_t EuclideanHarmonicSequencer::scaleDegreeToMidi(uint8_t degree, int8_t octaveShift) const {
  // Support arbitrary scale types (per-activeTrack)
  const ScaleDef &sd = SCALE_DEFS[(int)scaleType[activeTrack] % (sizeof(SCALE_DEFS)/sizeof(SCALE_DEFS[0]))];
  const int* scaleArr = sd.arr;
  uint8_t slen = sd.len;
  if (slen == 0) return 0;
  int cycles = degree / slen;
  int idx = degree % slen;
  // baseOctave is relative where 0 => C3 (MIDI 48). We add an offset so baseOctave 0 maps to octave 3 (C3 = 48).
  const int BASE_OCTAVE_ZERO = 4; // 4*12 = 48 -> C3 when tonic==0 and scaleArr[idx]==0
  int semitone = tonic[activeTrack] + scaleArr[idx] + (cycles + octaveShift) * 12 + (baseOctave[activeTrack] + BASE_OCTAVE_ZERO) * 12;
  return (uint8_t)constrain(semitone, 0, 127);
}

void EuclideanHarmonicSequencer::triggerChord(uint8_t degreeIndex) {
  if (!engine) return;
  // Determine which scale degree to use: prefer chordList for this track if available
  uint8_t scaleDegree = 0;
  uint8_t clSize = chordListSize[activeTrack];
  if (clSize > 0) {
    uint8_t cidx = chordListPos[activeTrack] % clSize;
    scaleDegree = chordList[activeTrack][cidx] % SCALE_LEN;
    // advance position for next hit
    chordListPos[activeTrack] = (uint8_t)((chordListPos[activeTrack] + 1) % clSize);
  } else {
    scaleDegree = degreeIndex % SCALE_LEN;
  }

  // Build chord with extensions based on polyphony for this track.
  uint8_t maxVoices = constrain(polyphony[activeTrack], (uint8_t)1, (uint8_t)EuclideanHarmonicSequencer::MAX_POLYPHONY);
  // Use fixed-size stack buffer to avoid dynamic allocations on heap
  uint8_t outNotesArr[EuclideanHarmonicSequencer::MAX_POLYPHONY];
  uint8_t outNotesCount = 0;
  // predefined offsets in scale-degree steps
  const int offs_count = 9;
  const int offs[offs_count] = {0, 2, 4, 6, 8, 8, 10, 12, 12};
  // flags indicating flattened (b9, b13) at positions 5 and 8 (0-based)
  const bool flattened[offs_count] = {false, false, false, false, false, true, false, false, true};

  for (uint8_t v = 0; v < maxVoices; ++v) {
    int idx = v;
    int octaveShift = 0;
    if (idx >= offs_count) {
      octaveShift = (idx - (offs_count-1)) / 1; // increase octave per extra voice
      idx = offs_count - 1;
    }
    int degOffset = offs[idx] + octaveShift * (int)SCALE_LEN;
    uint8_t baseMidi = scaleDegreeToMidi(scaleDegree + degOffset, 0);
    int midiNote = (int)baseMidi;
    if (flattened[idx]) midiNote = midiNote - 1;
    if (midiNote < 0) midiNote = 0; if (midiNote > 127) midiNote = 127;
    outNotesArr[outNotesCount++] = (uint8_t)midiNote;
  }

  uint16_t nl = noteLength[activeTrack];
  uint8_t ch = midiChannel[activeTrack] & 0x0F;
  uint8_t vel = velocity[activeTrack];
  if (distributionMode[activeTrack] == DIST_CHORDS) {
    for (uint8_t i = 0; i < outNotesCount; ++i) {
      engine->enqueueNoteEvent(ch, outNotesArr[i], vel, nl, true);
      PendingOff po{outNotesArr[i], ch, (uint32_t)millis() + nl};
      if (pendingOffsCount < EuclideanHarmonicSequencer::MAX_PENDING_OFFS) {
        pendingOffs[pendingOffsCount++] = po;
      }
    }
  } else {
    // NOTES: send only the root
    engine->enqueueNoteEvent(ch, outNotesArr[0], vel, nl, true);
    PendingOff po{outNotesArr[0], ch, (uint32_t)millis() + nl};
    if (pendingOffsCount < EuclideanHarmonicSequencer::MAX_PENDING_OFFS) {
      pendingOffs[pendingOffsCount++] = po;
    }
  }
}

void EuclideanHarmonicSequencer::fillChordListFromScale() {
  // fill for activeTrack
  chordListSize[activeTrack] = 0;
  for (uint8_t d=0; d<SCALE_LEN && chordListSize[activeTrack] < MAX_CHORDS; ++d) chordList[activeTrack][chordListSize[activeTrack]++] = d;
  chordListPos[activeTrack] = 0;
}

void EuclideanHarmonicSequencer::addChordDegree(uint8_t degree) {
  // Limit chord list to maximum 16 entries
  uint8_t &sz = chordListSize[activeTrack];
  if (sz >= MAX_CHORDS) return;
  chordList[activeTrack][sz++] = degree % SCALE_LEN;
}

void EuclideanHarmonicSequencer::removeLastChord() {
  uint8_t &sz = chordListSize[activeTrack];
  if (sz > 0) --sz;
  if (sz == 0) chordListPos[activeTrack] = 0;
  else if (chordListPos[activeTrack] >= sz) chordListPos[activeTrack] = chordListPos[activeTrack] % sz;
}

void EuclideanHarmonicSequencer::setChordListItem(uint8_t idx, uint8_t degree) {
  uint8_t sz = chordListSize[activeTrack];
  if (idx >= sz) return;
  chordList[activeTrack][idx] = degree % SCALE_LEN;
}

void EuclideanHarmonicSequencer::insertChordAt(uint8_t idx, uint8_t degree) {
  uint8_t &sz = chordListSize[activeTrack];
  if (sz >= MAX_CHORDS) return; // enforce max 16 chords
  if (idx > sz) idx = sz;
  for (int i = sz; i > (int)idx; --i) chordList[activeTrack][i] = chordList[activeTrack][i-1];
  chordList[activeTrack][idx] = degree % SCALE_LEN;
  ++sz;
}

void EuclideanHarmonicSequencer::removeChordAt(uint8_t idx) {
  uint8_t &sz = chordListSize[activeTrack];
  if (idx >= sz) return;
  for (uint8_t i = idx; i + 1 < sz; ++i) chordList[activeTrack][i] = chordList[activeTrack][i+1];
  if (sz > 0) --sz;
  if (sz == 0) chordListPos[activeTrack] = 0;
  else if (chordListPos[activeTrack] >= sz) chordListPos[activeTrack] = chordListPos[activeTrack] % sz;
}

void EuclideanHarmonicSequencer::moveChord(uint8_t idx, int8_t dir) {
  uint8_t sz = chordListSize[activeTrack];
  if (idx >= sz) return;
  int target = (int)idx + dir;
  if (target < 0 || target >= (int)sz) return;
  uint8_t tmp = chordList[activeTrack][idx];
  chordList[activeTrack][idx] = chordList[activeTrack][target];
  chordList[activeTrack][target] = tmp;
}

// Persistence removed: this sequencer uses RAM-only configuration.

void EuclideanHarmonicSequencer::getChordName(uint8_t chordIndex, char* out, size_t len) const {
  if (!out || len == 0) return;
  const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
  uint8_t degree = 0;
  uint8_t sz = chordListSize[activeTrack];
  if (chordIndex < sz) degree = chordList[activeTrack][chordIndex] % SCALE_LEN;
  // Determine root note (0-11)
  uint8_t root = scaleDegreeToMidi(degree, 0) % 12;
    // Base quality label (use 7th-quality labels where appropriate)
    const char* qual = "";
    if (majorScale[activeTrack]) {
      static const char* majQual[7] = {"M7","m7","m7","M7","7","m7","m7b5"};
      qual = majQual[degree % 7];
    } else {
      static const char* minQual[7] = {"m7","m7b5","M7","m7","m7","M7","7"};
      qual = minQual[degree % 7];
    }
    snprintf(out, len, "%s%s", noteNames[root], qual);
}

  void EuclideanHarmonicSequencer::formatChordName(uint8_t chordIndex, uint8_t poly, char* out, size_t len) const {
    if (!out || len == 0) return;
    // base name
    char base[32];
    getChordName(chordIndex, base, sizeof(base));
    // extensions mapping for positions >=5 (1-based poly positions)
    const char* extMap[] = { "9", "b9", "#9", "11", "#11", "13", "b13" };
    const uint8_t extCount = sizeof(extMap)/sizeof(extMap[0]);
    // first 4 voices are root,3,5,7. extensions start at voice index 5
    int toAdd = (int)poly - 4;
    if (toAdd <= 0) {
      snprintf(out, len, "%s", base);
      return;
    }
    // build suffix string
    char suf[64] = "";
    for (int i = 0; i < toAdd; ++i) {
      if (i >= extCount) break;
      if (suf[0] != '\0') strncat(suf, " ", sizeof(suf)-strlen(suf)-1);
      strncat(suf, extMap[i], sizeof(suf)-strlen(suf)-1);
    }
    if (suf[0] == '\0') {
      snprintf(out, len, "%s", base);
    } else {
      snprintf(out, len, "%s %s", base, suf);
    }
  }

void EuclideanHarmonicSequencer::update() {
  if (!midiClock) return;

  // If a forced feedback was requested (e.g., track change), send it here
  if (pendingFeedback) {
    pendingFeedback = false;
    MidiFeedback::sendAllHarmonicFeedbackForced(this, midiClock);
    OSCMapping::sendAllHarmonicFeedbackForced(this, midiClock);
  }

  // Process pending offs
  uint32_t now = millis();
  if (pendingOffsCount > 0) {
    uint8_t write = 0;
    for (uint8_t read = 0; read < pendingOffsCount; ++read) {
      PendingOff &p = pendingOffs[read];
      if ((int32_t)(now - p.time) >= 0) {
        if (engine) engine->enqueueNoteEvent(p.channel, p.note, 0, 0, false);
      } else {
        if (write != read) pendingOffs[write] = pendingOffs[read];
        ++write;
      }
    }
    pendingOffsCount = write;
  }

  // Process deferred pattern generation even if sequencer not running
  unsigned long nowMs = millis();
  for (uint8_t tt = 0; tt < MAX_TRACKS; ++tt) {
    if (patternDirty[tt]) {
      if ((unsigned long)(nowMs - patternLastEditTime[tt]) >= EuclideanHarmonicSequencer::PATTERN_DEBOUNCE_MS) {
        generatePatternForTrack(tt);
        patternDirty[tt] = false;
      }
    }
  }

  if (!running) return;

  // calcular passo atual baseado no MidiClock ticks
  uint32_t globalTicks = midiClock->getTickCount();
  // escolher ticks por step baseado no parâmetro Res: 1/4, 1/8, 1/16
  const uint16_t ticksPerRes[] = { 24, 12, 6 }; // PPQN ticks per harmonic step for 1/4,1/8,1/16
  // Para todas as tracks ativas, calcular passo e disparar acorde se necessário
  for (uint8_t t = 0; t < MAX_TRACKS; ++t) {
    if (!enabled[t]) continue;
    uint8_t ridx = resolutionIndex[t] % 3;
    uint16_t ticksPerStep = ticksPerRes[ridx];
    uint8_t s = steps[t];
    if (s == 0) s = 1;
    uint8_t step = (globalTicks / ticksPerStep) % s;
    // Só dispara se mudou de step para esta track
    static uint8_t lastStepPerTrack[MAX_TRACKS] = {255,255,255,255,255,255,255,255};
    if (step != lastStepPerTrack[t]) {
      lastStepPerTrack[t] = step;
      // Reset no início de cada ciclo para tocar sempre do primeiro acorde
      if (step == 0) {
        chordListPos[t] = 0;
      }
      if (step < patternLen[t] && pattern[t][step]) {
        // Temporariamente muda activeTrack para t para usar triggerChord corretamente
        uint8_t prev = activeTrack;
        activeTrack = t;
        triggerChord(step);
        activeTrack = prev;
      }
      // Atualiza currentStep global para UI (mostra ponteiro da última track processada)
      if (t == activeTrack) currentStep = step;
      // Atualiza passo por track para UI
      currentStepPerTrack[t] = step;
    }
  }
    // Atualiza currentStep global para refletir o primeiro track ativo (coerência para UI quando necessário)
    bool found = false;
    for (uint8_t t = 0; t < MAX_TRACKS; ++t) {
      if (enabled[t]) {
        currentStep = currentStepPerTrack[t];
        found = true;
        break;
      }
    }
    if (!found) currentStep = 0;
}
