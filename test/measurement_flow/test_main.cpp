#include "MeasurementFlow.h"
#include <cstdio>
#include <cstdlib>
using namespace lap_timer;
static unsigned checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
  std::fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); std::exit(1); \
} } while (false)
static SensorSample sample(std::uint16_t mm, std::uint64_t us,
                           SampleStatus status = SampleStatus::Ok) {
  SensorSample s;
  s.distanceMm = mm; s.readEndUs = us; s.status = status;
  return s;
}
static void arm(LapTimer& t) {
  t.onAButton();
  applyCompletedSample(t, sample(123, 1), false);
  for (unsigned i = 0; i < 10; ++i)
    applyCompletedSample(t, sample(300, i + 2), false);
  CHECK(t.context().state == State::Armed);
}
int main() {
  CHECK(classifyReading(true, 5, UINT16_MAX) == SampleStatus::Timeout);
  CHECK(classifyReading(false, 5, UINT16_MAX) == SampleStatus::I2cError);
  CHECK(classifyReading(false, 0, UINT16_MAX) == SampleStatus::ReadError);
  CHECK(classifyReading(false, 0, 300) == SampleStatus::Ok);
  CHECK(sampleError(SampleStatus::Timeout) == Error::SensorTimeout);
  CHECK(sampleError(SampleStatus::I2cError) == Error::SensorI2c);
  CHECK(sampleError(SampleStatus::InitError) == Error::SensorInit);
  CHECK(sampleError(SampleStatus::ConfigError) == Error::SensorConfig);
  CHECK(sampleError(SampleStatus::NotRun) == Error::SensorRead);
  CHECK(sampleError(SampleStatus::Ok) == Error::None);
  LapTimer t;
  arm(t); // A before read: one discard, then 10 samples.
  applyCompletedSample(t, sample(280, 100), false);
  CHECK(t.context().state == State::Running);
  applyCompletedSample(t, sample(300, 101), false);
  auto generation = t.context().generation;
  applyCompletedSample(t, sample(280, 1000100), true); // A during potential lap.
  CHECK(t.context().generation == generation + 1);
  CHECK(t.context().state == State::Calibrating);
  CHECK(t.context().lapCount == 0);
  CHECK(t.context().calibrationCount == 0);
  CHECK(t.context().baselineSumMm == 0);
  CHECK(t.context().discardPending);
  applyCompletedSample(t, sample(900, 1000101), false);
  CHECK(t.context().calibrationCount == 0);
  for (unsigned i = 0; i < 5; ++i)
    applyCompletedSample(t, sample(300, 1000102 + i), false);
  CHECK(t.context().calibrationCount == 5);
  applyCompletedSample(t, sample(800, 1000200), true); // Cancel mid-calibration.
  CHECK(t.context().calibrationCount == 0);
  CHECK(t.context().baselineSumMm == 0);
  arm(t);
  applyCompletedSample(t, sample(280, 200), false);
  applyCompletedSample(t, sample(300, 201), false);
  applyCompletedSample(t, sample(280, 1000200), false);
  CHECK(t.context().lapCount == 1);
  generation = t.context().generation;
  applyCompletedSample(t, sample(0, 1500200, SampleStatus::Timeout), true);
  CHECK(t.context().state == State::Error); // Error wins over A.
  CHECK(t.context().error == Error::SensorTimeout);
  CHECK(t.context().generation == generation);
  CHECK(t.context().lapCount == 1);
  CHECK(t.elapsedUs(9000000) == 1500000);
  applyCompletedSample(t, sample(0, 1800200, SampleStatus::I2cError), false);
  CHECK(t.elapsedUs(9000000) == 1500000);
  // A after acquisition cancels the current session before the next read.
  t.onAButton();
  CHECK(t.context().error == Error::None);
  CHECK(t.context().lapCount == 0);
  applyCompletedSample(t, sample(200, 1800201), false);
  CHECK(t.context().calibrationCount == 0);
  for (unsigned i = 0; i < 10; ++i)
    applyCompletedSample(t, sample(300, 1800202 + i), false);
  CHECK(t.context().state == State::Armed);
  std::printf("PASS: measurement flow, %u checks\n", checks);
}
