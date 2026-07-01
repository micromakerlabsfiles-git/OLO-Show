// OLO Show Dynamic Videos Registry
#ifndef GENERATED_VIDEOS_H
#define GENERATED_VIDEOS_H

#include <Arduino.h>

#include "video1.h"

#define MAX_ANIM_SLOTS 1

struct SlotInfo {
  const unsigned char (*frames)[1024];
  int total_frames;
  int frame_delay;
};

const SlotInfo SLOT_TABLE[MAX_ANIM_SLOTS] = {
  { video1_frames, VIDEO1_TOTAL_FRAMES, VIDEO1_FRAME_DELAY },
};

#endif // GENERATED_VIDEOS_H
