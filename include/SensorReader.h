#pragma once
#include <VL53L0X.h>
#include "SensorTypes.h"

namespace lap_timer {
constexpr std::uint32_t kI2cFrequencyHz = 400000;
constexpr std::uint32_t kTimingBudgetUs = 20000;
class SensorReader {
 public:
  Error initialize();
  SensorSample collectSample();
  bool ready() const { return startupStatus_ == SampleStatus::Ok; }
  Error startupError() const { return sampleError(startupStatus_); }
  std::uint32_t actualTimingBudgetUs() const { return actualTimingBudgetUs_; }
  static std::uint64_t nowUs();

 private:
  VL53L0X sensor_;
  bool initializationAttempted_ = false;
  SampleStatus startupStatus_ = SampleStatus::InitError;
  std::uint32_t actualTimingBudgetUs_ = 0;
};
}  // namespace lap_timer
