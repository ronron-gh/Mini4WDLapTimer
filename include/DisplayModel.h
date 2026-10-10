#pragma once
#include "LapTimer.h"
namespace lap_timer {
struct DisplayRow {
  bool visible = false;
  bool confirmed = false;
  std::uint64_t lapCs = 0;
  std::uint64_t elapsedCs = 0;
};
struct DisplayModel {
  State state = State::Idle;
  Error error = Error::None;
  std::uint64_t generation = 0;
  std::size_t lapCount = 0;
  std::size_t calibrationCount = 0;
  std::uint32_t baselineSumMm = 0;
  bool baselineReady = false;
  std::uint64_t elapsedCs = 0;
  std::array<DisplayRow, kLapCount> rows{};
};
DisplayModel makeDisplayModel(const TimerContext& context, std::uint64_t nowUs);
bool sameDisplayModel(const DisplayModel& a, const DisplayModel& b);
bool sameRow(const DisplayRow& a, const DisplayRow& b);
// Returns false if the destination is too small; never silently truncates.
bool formatCentiseconds(std::uint64_t cs, char* destination, std::size_t size);
bool formatSeconds(std::uint64_t us, char* destination, std::size_t size);
bool formatBaselineDistance(const DisplayModel& model, char* destination, std::size_t size);
const char* stateName(State state);
const char* stateLabel(State state);
const char* errorName(Error error);
const char* actionName(State state);
const char* actionLabel(State state);
}  // namespace lap_timer
