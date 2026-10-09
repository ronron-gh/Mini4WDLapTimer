#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace lap_timer {
constexpr std::size_t kLapCount = 10;
constexpr std::size_t kBaselineSamples = 10;
constexpr std::int32_t kDetectDeltaMm = 20;
constexpr std::int32_t kReleaseDeltaMm = 10;
constexpr std::uint64_t kDetectionLockoutUs = 1000000;

enum class State : std::uint8_t { Idle, Calibrating, Armed, Running, Complete, Error };
enum class Error : std::uint8_t { None, SensorInit, SensorConfig, SensorTimeout, SensorI2c, SensorRead };

struct LapRecord {
  std::uint64_t lapUs = 0;
  std::uint64_t elapsedUs = 0;
  bool confirmed = false;
};

struct TimerContext {
  State state = State::Idle;
  Error error = Error::None;
  std::uint64_t generation = 0;
  std::uint32_t baselineSumMm = 0;
  std::size_t calibrationCount = 0;
  bool discardPending = false;
  bool vehiclePresent = false;
  std::uint64_t startUs = 0;
  std::uint64_t previousPassUs = 0;
  std::uint64_t frozenElapsedUs = 0;
  std::size_t lapCount = 0;
  std::array<LapRecord, kLapCount> laps{};
};

class LapTimer {
 public:
  // Startup only; runtime recovery must use onAButton(), not initialize().
  void initialize(Error startupError = Error::None);
  void onAButton();
  // Caller supplies monotonic API-return times and must not submit a sample
  // belonging to the old session after processing an A-button cancellation.
  void onSample(std::uint16_t distanceMm, std::uint64_t readEndUs,
                Error error = Error::None);
  std::uint64_t elapsedUs(std::uint64_t nowUs) const;
  const TimerContext& context() const { return context_; }

 private:
  void enterError(Error error, std::uint64_t timeUs);
  void updateDetector(std::uint16_t distanceMm, std::uint64_t timeUs);
  void handlePass(std::uint64_t timeUs);
  TimerContext context_{};
  Error startupError_ = Error::None;
};
}  // namespace lap_timer
