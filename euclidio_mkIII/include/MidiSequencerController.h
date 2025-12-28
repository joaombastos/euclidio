#ifndef MIDI_SEQUENCER_CONTROLLER_H
#define MIDI_SEQUENCER_CONTROLLER_H

#include <stdint.h>

class EuclideanSequencer;

class MidiSequencerController {
public:
  // Mapeia MIDI CC para controle do sequenciador
  // CC 20-26: Steps, Hits, Offset, Note, Channel, Resolution, Track
  static void processControlChange(uint8_t cc, uint8_t value, EuclideanSequencer* seq);
  
private:
  static const uint8_t CC_STEPS = 20;
  static const uint8_t CC_HITS = 21;
  static const uint8_t CC_OFFSET = 22;
  static const uint8_t CC_NOTE = 23;
  static const uint8_t CC_CHANNEL = 24;
  static const uint8_t CC_RESOLUTION = 25;
  static const uint8_t CC_TRACK = 26;
};

#endif // MIDI_SEQUENCER_CONTROLLER_H
