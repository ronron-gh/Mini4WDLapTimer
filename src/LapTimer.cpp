#include "LapTimer.h"

namespace lap_timer {
void LapTimer::initialize(Error startupError) {
  context_ = TimerContext{};
  startupError_ = startupError;
  if (startupError != Error::None) {
    context_.state = State::Error;
    context_.error = startupError;
  }
}

void LapTimer::onAButton() {
  const auto generation = context_.generation + 1;
  context_ = TimerContext{};
  context_.generation = generation;
  if (startupError_ != Error::None) {
    context_.state = State::Error;
    context_.error = startupError_;
    return;
  }
  context_.state = State::Calibrating;
  context_.discardPending = true;
}

void LapTimer::onSample(std::uint16_t distanceMm, std::uint64_t timeUs, Error error) {
  if (startupError_ != Error::None || context_.state == State::Error) return;
  if (error == Error::None && distanceMm == UINT16_MAX) error = Error::SensorRead;
  if (error != Error::None) {
    enterError(error, timeUs);
    return;
  }
  if (context_.state == State::Calibrating) {
    if (context_.discardPending) {
      context_.discardPending = false;
      return;
    }
    context_.baselineSumMm += distanceMm;
    if (++context_.calibrationCount == kBaselineSamples) {
      context_.state = State::Armed;
    }
    return;
  }
  if (context_.state == State::Armed || context_.state == State::Running) {
    updateDetector(distanceMm, timeUs);
  }
}

void LapTimer::updateDetector(std::uint16_t distanceMm, std::uint64_t timeUs) {
  // Preserve fractional averages and allow distances beyond the baseline.
  const auto delta10 = static_cast<std::int32_t>(context_.baselineSumMm) -
                       static_cast<std::int32_t>(kBaselineSamples) * distanceMm;
  if (context_.vehiclePresent) {
    if (delta10 <= kReleaseDeltaMm * static_cast<std::int32_t>(kBaselineSamples)) {
      context_.vehiclePresent = false;
    }
    return;
  }
  if (delta10 < kDetectDeltaMm * static_cast<std::int32_t>(kBaselineSamples)) return;
  // Latch even rejected entries so they are not counted later while stationary.
  context_.vehiclePresent = true;
  if (context_.state == State::Running &&
      timeUs - context_.previousPassUs < kDetectionLockoutUs) return;
  handlePass(timeUs);
}

void LapTimer::handlePass(std::uint64_t timeUs) {
  if (context_.state == State::Armed) {
    context_.startUs = timeUs;
    context_.previousPassUs = timeUs;
    context_.state = State::Running;
    return;
  }
  auto& lap = context_.laps[context_.lapCount++];
  lap.lapUs = timeUs - context_.previousPassUs;
  lap.elapsedUs = timeUs - context_.startUs;
  lap.confirmed = true;
  context_.previousPassUs = timeUs;
  if (context_.lapCount == kLapCount) {
    context_.frozenElapsedUs = lap.elapsedUs;
    context_.state = State::Complete;
  }
}

std::uint64_t LapTimer::elapsedUs(std::uint64_t nowUs) const {
  if (context_.state == State::Running) return nowUs - context_.startUs;
  return context_.frozenElapsedUs;
}

void LapTimer::enterError(Error error, std::uint64_t timeUs) {
  context_.frozenElapsedUs = elapsedUs(timeUs);
  context_.state = State::Error;
  context_.error = error;
}
}  // namespace lap_timer
