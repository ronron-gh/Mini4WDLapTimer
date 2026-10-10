#pragma once
#include <array>
#include "SensorTypes.h"
namespace lap_timer {
struct BenchmarkStatistics {
  std::uint64_t sum = 0;
  std::uint64_t minimum = UINT64_MAX;
  std::uint64_t maximum = 0;
  std::size_t count = 0;
  void add(std::uint64_t value);
  double mean() const { return count ? static_cast<double>(sum) / count : 0; }
};
struct BenchmarkSummary {
  std::size_t attempted = 0;
  std::size_t successful = 0;
  BenchmarkStatistics durations;
  BenchmarkStatistics intervals;
  SampleStatus error = SampleStatus::Ok;
  bool preparationFailed = false;
};
class BenchmarkData {
 public:
  void begin();
  void consume(const SensorSample& sample);
  void failPreparation();
  bool done() const { return done_; }
  const std::array<SensorSample, 50>& samples() const { return samples_; }
  BenchmarkSummary summary() const;
 private:
  std::array<SensorSample, 50> samples_{};
  std::size_t count_ = 0;
  bool discard_ = true;
  bool done_ = false;
  bool preparationFailed_ = false;
  SampleStatus error_ = SampleStatus::Ok;
};
}  // namespace lap_timer
