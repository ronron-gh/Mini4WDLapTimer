#pragma once
#include <array>
#include <Stream.h>
#include "SensorReader.h"
namespace lap_timer {
class Benchmark {
 public:
  // Measurement task only; caller must silence output before capture.
  void capture(SensorReader& reader);
  // Presentation task only, after capture has finished and the buffer is immutable.
  void printResults(Stream& output, const SensorReader& reader) const;
  const std::array<SensorSample, 50>& samples() const { return samples_; }
 private:
  std::array<SensorSample, 50> samples_{};
  SampleStatus batchError_ = SampleStatus::Ok;
};
}  // namespace lap_timer
