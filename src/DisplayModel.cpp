#include "DisplayModel.h"
#include <cstdio>
namespace lap_timer {
DisplayModel makeDisplayModel(const TimerContext& context, std::uint64_t nowUs) {
  DisplayModel model;
  model.state = context.state;
  model.error = context.error;
  model.generation = context.generation;
  model.lapCount = context.lapCount;
  model.calibrationCount = context.calibrationCount;
  model.baselineSumMm = context.baselineSumMm;
  model.baselineReady = context.calibrationCount == kBaselineSamples;
  const auto elapsed = context.state == State::Running
      ? (nowUs >= context.startUs ? nowUs - context.startUs : 0)
      : context.frozenElapsedUs;
  model.elapsedCs = elapsed / 10000;
  for (std::size_t i = 0; i < kLapCount; ++i) {
    const auto& lap = context.laps[i];
    auto& row = model.rows[i];
    row.confirmed = lap.confirmed;
    row.visible = lap.confirmed ||
        (i == 0 && (context.state == State::Idle || context.state == State::Calibrating));
    if (lap.confirmed) {
      row.lapCs = lap.lapUs / 10000;
      row.elapsedCs = lap.elapsedUs / 10000;
    }
  }
  return model;
}
bool sameRow(const DisplayRow& a, const DisplayRow& b) {
  return a.visible == b.visible && a.confirmed == b.confirmed &&
         a.lapCs == b.lapCs && a.elapsedCs == b.elapsedCs;
}
bool sameDisplayModel(const DisplayModel& a, const DisplayModel& b) {
  if (a.state != b.state || a.error != b.error || a.generation != b.generation ||
      a.lapCount != b.lapCount || a.calibrationCount != b.calibrationCount ||
      a.elapsedCs != b.elapsedCs || a.baselineSumMm != b.baselineSumMm ||
      a.baselineReady != b.baselineReady) return false;
  for (std::size_t i = 0; i < kLapCount; ++i)
    if (!sameRow(a.rows[i], b.rows[i])) return false;
  return true;
}
bool formatCentiseconds(std::uint64_t cs, char* destination, std::size_t size) {
  if (!destination || size == 0) return false;
  const int count = std::snprintf(destination, size, "%llu.%02u",
      static_cast<unsigned long long>(cs / 100), static_cast<unsigned>(cs % 100));
  if (count < 0 || static_cast<std::size_t>(count) >= size) {
    destination[0] = '\0';
    return false;
  }
  return true;
}
bool formatSeconds(std::uint64_t us, char* destination, std::size_t size) {
  return formatCentiseconds(us / 10000, destination, size);
}
bool formatBaselineDistance(const DisplayModel& model, char* destination, std::size_t size) {
  if (!destination || size == 0) return false;
  destination[0] = '\0';
  if (!model.baselineReady) return false;
  const int count = std::snprintf(destination, size, "%lu.%lu",
      static_cast<unsigned long>(model.baselineSumMm / kBaselineSamples),
      static_cast<unsigned long>(model.baselineSumMm % kBaselineSamples));
  if (count < 0 || static_cast<std::size_t>(count) >= size) {
    destination[0] = '\0';
    return false;
  }
  return true;
}
const char* stateName(State state) {
  switch (state) {
    case State::Idle: return "IDLE";
    case State::Calibrating: return "CALIBRATING";
    case State::Armed: return "ARMED";
    case State::Running: return "RUNNING";
    case State::Complete: return "COMPLETE";
    default: return "ERROR";
  }
}
const char* stateLabel(State state) {
  switch (state) {
    case State::Idle: return "待機";
    case State::Calibrating: return "基準距離取得";
    case State::Armed: return "スタート待ち";
    case State::Running: return "計測中";
    case State::Complete: return "完了";
    default: return "エラー";
  }
}
const char* errorName(Error error) {
  switch (error) {
    case Error::None: return "NONE";
    case Error::SensorInit: return "SENSOR_INIT";
    case Error::SensorConfig: return "SENSOR_CONFIG";
    case Error::SensorTimeout: return "SENSOR_TIMEOUT";
    case Error::SensorI2c: return "SENSOR_I2C";
    default: return "SENSOR_READ";
  }
}
const char* actionName(State state) { return state == State::Idle ? "START" : "RESET"; }
const char* actionLabel(State state) { return state == State::Idle ? "スタート" : "リセット"; }
}  // namespace lap_timer
