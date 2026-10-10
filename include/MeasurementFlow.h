#pragma once
#include "SensorTypes.h"

namespace lap_timer {
// Called after reading the post-acquisition A notification. Never submit an
// old successful reading to the new calibration or the old lap calculation.
inline void applyCompletedSample(LapTimer& timer, const SensorSample& sample,
                                 bool aPressedDuringRead) {
  const auto error = sampleError(sample.status);
  if (error != Error::None) {
    timer.onSample(sample.distanceMm, sample.readEndUs, error);
  } else if (aPressedDuringRead) {
    timer.onAButton();
  } else {
    timer.onSample(sample.distanceMm, sample.readEndUs);
  }
}
}  // namespace lap_timer
