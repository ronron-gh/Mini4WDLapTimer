#include "SensorReader.h"
#include <Wire.h>
#include <esp_timer.h>

namespace lap_timer {
std::uint64_t SensorReader::nowUs() {
  return static_cast<std::uint64_t>(esp_timer_get_time());
}
Error SensorReader::initialize() {
  if (initializationAttempted_) return startupError();
  initializationAttempted_ = true;
  const bool busReady = Wire.begin(21, 22) && Wire.setClock(kI2cFrequencyHz);
  Wire.setTimeOut(20);
  sensor_.setBus(&Wire);
  sensor_.setTimeout(1000);
  if (!busReady || !sensor_.init() || sensor_.last_status != 0) return startupError();
  startupStatus_ = SampleStatus::ConfigError;
  if (!sensor_.setMeasurementTimingBudget(kTimingBudgetUs) || sensor_.last_status != 0)
    return startupError();
  startupStatus_ = SampleStatus::I2cError;
  actualTimingBudgetUs_ = sensor_.getMeasurementTimingBudget();
  if (sensor_.last_status != 0) return startupError();
  sensor_.timeoutOccurred();
  sensor_.startContinuous();
  if (sensor_.last_status != 0) return startupError();
  startupStatus_ = SampleStatus::Ok;
  return Error::None;
}
SensorSample SensorReader::collectSample() {
  SensorSample sample;
  sample.readStartUs = nowUs();
  if (!ready()) {
    sample.readEndUs = sample.readStartUs;
    sample.status = startupStatus_;
    return sample;
  }
  sample.distanceMm = sensor_.readRangeContinuousMillimeters();
  sample.readEndUs = nowUs();
  sample.status = classifyReading(sensor_.timeoutOccurred(), sensor_.last_status,
                                  sample.distanceMm);
  return sample;
}
}  // namespace lap_timer
