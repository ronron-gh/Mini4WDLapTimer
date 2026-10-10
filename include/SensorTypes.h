#pragma once
#include "LapTimer.h"

namespace lap_timer {
enum class SampleStatus : std::uint8_t {
  Ok, Timeout, I2cError, ReadError, InitError, ConfigError, NotRun
};
struct SensorSample {
  std::uint64_t readStartUs = 0;
  std::uint64_t readEndUs = 0;
  std::uint16_t distanceMm = 0;
  SampleStatus status = SampleStatus::NotRun;
};
inline SampleStatus classifyReading(bool timeout, std::uint8_t lastStatus,
                                    std::uint16_t distance) {
  if (timeout) return SampleStatus::Timeout;
  if (lastStatus != 0) return SampleStatus::I2cError;
  if (distance == UINT16_MAX) return SampleStatus::ReadError;
  return SampleStatus::Ok;
}
inline Error sampleError(SampleStatus status) {
  switch (status) {
    case SampleStatus::Ok: return Error::None;
    case SampleStatus::Timeout: return Error::SensorTimeout;
    case SampleStatus::I2cError: return Error::SensorI2c;
    case SampleStatus::InitError: return Error::SensorInit;
    case SampleStatus::ConfigError: return Error::SensorConfig;
    default: return Error::SensorRead;
  }
}
inline const char* sampleStatusName(SampleStatus status) {
  switch (status) {
    case SampleStatus::Ok: return "OK";
    case SampleStatus::Timeout: return "TIMEOUT";
    case SampleStatus::I2cError: return "I2C_ERROR";
    case SampleStatus::ReadError: return "READ_ERROR";
    case SampleStatus::InitError: return "INIT_ERROR";
    case SampleStatus::ConfigError: return "CONFIG_ERROR";
    default: return "NOT_RUN";
  }
}
}  // namespace lap_timer
