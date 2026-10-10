#pragma once
#include "BenchmarkSession.h"
#include "BenchmarkData.h"
namespace lap_timer {
// Return true whenever the post-read path must publish a new UI snapshot.
inline bool processBenchmarkSample(BenchmarkSession& session, BenchmarkData& data,
                                const SensorSample& sample, bool aPressed) {
  const auto mode = session.mode();
  const auto run = session.run();
  session.requestAgain(aPressed, sample.readEndUs);
  if (session.mode() == BenchmarkPhase::Capturing) {
    data.consume(sample);
    if (data.done()) session.captureDone();
  }
  return mode != session.mode() || run != session.run();
}
}  // namespace lap_timer
