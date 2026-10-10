#pragma once
#include <array>
#include <Stream.h>
#include "SensorReader.h"
#include "BenchmarkData.h"
#include "BenchmarkAcquisition.h"
namespace lap_timer {
class Benchmark {
 public:
  // Measurement task only; caller must silence output before capture.
  void capture(SensorReader& reader);
  void begin() { data_.begin(); }
  void consume(const SensorSample& sample) { data_.consume(sample); }
  bool processSample(BenchmarkSession& session, const SensorSample& sample, bool aPressed) {
    return processBenchmarkSample(session, data_, sample, aPressed);
  }
  void failPreparation() { data_.failPreparation(); }
  bool done() const { return data_.done(); }
  BenchmarkSummary summary() const { return data_.summary(); }
  // Presentation task only, after capture has finished and the buffer is immutable.
  void printResults(Stream& output, const SensorReader& reader) const;
  const std::array<SensorSample, 50>& samples() const { return data_.samples(); }
 private:
  BenchmarkData data_;
};
}  // namespace lap_timer
