#include "MidiSequencerController.h"
#include "EuclideanSequencer.h"

void MidiSequencerController::processControlChange(uint8_t cc, uint8_t value, EuclideanSequencer* seq) {
  if (!seq) return;
  
  // Converte valor MIDI (0-127) para range apropriado
  switch (cc) {
    case CC_STEPS:
      // 0-127 mapeado para 1-32
      seq->setSteps(constrain(map(value, 0, 127, 1, 32), 1, 32));
      break;
      
    case CC_HITS:
      // 0-127 mapeado para 1-32
      seq->setHits(constrain(map(value, 0, 127, 1, 32), 1, 32));
      break;
      
    case CC_OFFSET:
      // 0-127 mapeado para 0-31 (offset máximo é steps-1)
      seq->setOffset(map(value, 0, 127, 0, 31));
      break;
      
    case CC_NOTE:
      // 0-127 mapeado para 0-127 (nota MIDI completa)
      seq->setNote(value);
      break;
      
    case CC_RESOLUTION:
      // 0-127 mapeado para 1-4
      // 0-31: res 1, 32-63: res 2, 64-95: res 3, 96-127: res 4
      {
        uint8_t res = constrain(map(value, 0, 127, 1, 4), 1, 4);
        seq->setResolution(res);
      }
      break;
      
    case CC_TRACK:
      // 0-127 mapeado para 0-7 (8 tracks)
      seq->setSelectedPattern(map(value, 0, 127, 0, 7));
      break;
      
    // CC_PLAY handling removed: Play/Stop must be triggered only via NOTE 77 (NOTE_TOGGLE_PLAY)
  }
}
