#pragma once
#include "HiddenBenchmark.h"
#include "BenchmarkData.h"
namespace lap_timer {
// Return true whenever the post-read path must publish a new UI snapshot.
inline bool processHiddenSample(HiddenBenchmark& hidden, BenchmarkData& data,
                                const SensorSample& sample, bool aPressed) {
  const auto mode = hidden.mode();
  const auto run = hidden.run();
  hidden.request(aPressed, false, false, sample.readEndUs);
  if (hidden.mode() == HiddenMode::Capturing) {
    data.consume(sample);
    if (data.done()) hidden.captureDone();
  }
  return mode != hidden.mode() || run != hidden.run();
}
}  // namespace lap_timer
