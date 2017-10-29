#ifndef __TEENSY_SPEAKER_H
#define __TEENSY_SPEAKER_H

#include "physicalspeaker.h"

class TeensySpeaker : public PhysicalSpeaker {
 public:
  TeensySpeaker(uint8_t pinNum);
  virtual ~TeensySpeaker();

  virtual void toggle();
  virtual void maintainSpeaker(uint32_t c);

  virtual void beginMixing();
  virtual void mixOutput(uint8_t v);

 private:
  uint8_t speakerPin;

  bool toggleState;
  bool needsToggle;

  uint32_t mixerValue;
  uint8_t numMixed;
};

#endif
