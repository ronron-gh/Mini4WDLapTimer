#include "LapTimer.h"

#include <cstdio>
#include <cstdlib>

using namespace lap_timer;
static unsigned checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
  std::fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
  std::exit(1); } } while (false)

static void calibrate(LapTimer& timer, std::uint16_t mm = 300) {
  timer.onAButton();
  timer.onSample(123, 0);  // Pending reading is not part of the mean.
  for (unsigned i = 0; i < 10; ++i) timer.onSample(mm, i + 1);
  CHECK(timer.context().state == State::Armed);
}

static void complete(LapTimer& timer) {
  calibrate(timer);
  timer.onSample(280, 100);
  for (unsigned i = 1; i <= 10; ++i) {
    timer.onSample(300, 100 + i * 1000000ULL - 1);
    timer.onSample(280, 100 + i * 1000000ULL);
  }
  CHECK(timer.context().state == State::Complete);
}

static void testCalibration() {
  LapTimer t;
  CHECK(t.context().state == State::Idle);
  t.onSample(0, 1);
  CHECK(t.context().state == State::Idle);
  CHECK(t.elapsedUs(2) == 0);
  t.onAButton();
  CHECK(t.context().discardPending);
  t.onSample(0, 3);
  CHECK(t.context().calibrationCount == 0);
  for (unsigned i = 0; i < 9; ++i) t.onSample(300, 4 + i);
  CHECK(t.context().state == State::Calibrating);
  t.onSample(305, 13);
  CHECK(t.context().state == State::Armed);
  CHECK(t.context().baselineSumMm == 3005);
  t.onSample(281, 14); // 19.5 mm is below the threshold.
  CHECK(t.context().state == State::Armed);
  t.onSample(280, 15); // 20.5 mm is above the threshold.
  CHECK(t.context().state == State::Running);
}

static void testHysteresisAndLongDistance() {
  LapTimer t;
  calibrate(t);
  t.onSample(65534, 20);
  CHECK(t.context().state == State::Armed);
  t.onSample(281, 21);
  CHECK(t.context().state == State::Armed);
  t.onSample(280, 22);
  CHECK(t.context().state == State::Running);
  t.onSample(280, 2000022);
  CHECK(t.context().lapCount == 0); // Staying under the sensor is not another pass.
  t.onSample(289, 2000023);
  CHECK(t.context().vehiclePresent); // 11 mm does not release.
  t.onSample(280, 2000024);
  CHECK(t.context().lapCount == 0);
  t.onSample(290, 2000025);
  CHECK(!t.context().vehiclePresent); // 10 mm releases.
  t.onSample(280, 2000026);
  CHECK(t.context().lapCount == 1);
  CHECK(t.context().laps[0].lapUs == 2000004);
}

static void testLockout() {
  LapTimer t;
  calibrate(t);
  t.onSample(280, 100);
  t.onSample(300, 101);
  t.onSample(280, 1000099); // 999999 us after start.
  CHECK(t.context().lapCount == 0);
  t.onSample(280, 1000100);
  CHECK(t.context().lapCount == 0); // No late detection of rejected entry.
  t.onSample(300, 1000100);
  t.onSample(280, 1000100); // Exactly 1000000 us, new entry.
  CHECK(t.context().lapCount == 1);
  CHECK(t.context().laps[0].lapUs == 1000000);
  CHECK(t.context().laps[0].elapsedUs == 1000000);
}

static void testTenLaps() {
  LapTimer t;
  complete(t);
  CHECK(t.context().lapCount == 10);
  for (unsigned i = 0; i < 10; ++i) {
    CHECK(t.context().laps[i].confirmed);
    CHECK(t.context().laps[i].lapUs == 1000000);
    CHECK(t.context().laps[i].elapsedUs == (i + 1) * 1000000ULL);
  }
  t.onSample(300, 11000100);
  t.onSample(280, 12000100);
  CHECK(t.context().lapCount == 10);
  CHECK(t.elapsedUs(99000100) == 10000000);
}

static void testResetEveryState() {
  for (unsigned stage = 0; stage < 6; ++stage) {
    LapTimer t;
    if (stage == 1) { t.onAButton(); t.onSample(0, 0); t.onSample(300, 1); }
    if (stage == 2) calibrate(t);
    if (stage == 3) { calibrate(t); t.onSample(280, 100); }
    if (stage == 4) complete(t);
    if (stage == 5) t.onSample(0, 100, Error::SensorTimeout);
    auto oldGeneration = t.context().generation;
    t.onAButton();
    CHECK(t.context().state == State::Calibrating);
    CHECK(t.context().generation == oldGeneration + 1);
    CHECK(t.context().calibrationCount == 0);
    CHECK(t.context().baselineSumMm == 0);
    CHECK(t.context().discardPending);
    CHECK(t.context().lapCount == 0);
    CHECK(t.context().error == Error::None);
    CHECK(t.elapsedUs(123456) == 0);
    for (const auto& lap : t.context().laps) CHECK(!lap.confirmed);
  }
}

static void testErrors() {
  for (unsigned position = 0; position < 11; ++position) {
    LapTimer t;
    t.onAButton();
    for (unsigned i = 0; i < position; ++i) t.onSample(300, i);
    t.onSample(0, 20, Error::SensorI2c);
    CHECK(t.context().state == State::Error);
    CHECK(t.context().error == Error::SensorI2c);
    CHECK(t.context().calibrationCount < 10);
    calibrate(t);
  }
  LapTimer t;
  calibrate(t);
  t.onSample(280, 100);
  t.onSample(300, 101);
  t.onSample(280, 1000100);
  t.onSample(0, 1500100, Error::SensorTimeout);
  CHECK(t.elapsedUs(9999999) == 1500000);
  CHECK(t.context().lapCount == 1);
  CHECK(t.context().laps[0].confirmed);
  t.onSample(0, 1800100, Error::SensorI2c);
  CHECK(t.elapsedUs(9999999) == 1500000);
  CHECK(t.context().error == Error::SensorTimeout);
  calibrate(t);
  CHECK(t.context().lapCount == 0);
  t.onSample(UINT16_MAX, 100);
  CHECK(t.context().error == Error::SensorRead);
  complete(t);
  t.onSample(0, 12000100, Error::SensorRead);
  CHECK(t.context().state == State::Error);
  CHECK(t.context().lapCount == 10);
  CHECK(t.elapsedUs(99999999) == 10000000);
}

static void testStartupFailure() {
  for (Error e : {Error::SensorInit, Error::SensorConfig, Error::SensorI2c}) {
    LapTimer t;
    t.initialize(e);
    t.onAButton();
    t.onSample(300, 1);
    CHECK(t.context().state == State::Error);
    CHECK(t.context().error == e);
    CHECK(t.context().calibrationCount == 0);
  }
}

static void testWideTimestampsAndPrecision() {
  LapTimer t;
  calibrate(t);
  constexpr std::uint64_t start = 0x100000000ULL + 123;
  t.onSample(280, start);
  CHECK(t.elapsedUs(start + 1234567) == 1234567);
  t.onSample(300, start + 1234566);
  t.onSample(280, start + 1234567);
  CHECK(t.context().laps[0].lapUs == 1234567);
  CHECK(t.context().laps[0].elapsedUs == 1234567);
  t.onSample(300, start + 1234568);
  t.onSample(280, start + 5000000000ULL);
  CHECK(t.context().laps[1].lapUs == 5000000000ULL - 1234567);
  CHECK(t.context().laps[1].elapsedUs == 5000000000ULL);
}

int main() {
  testCalibration();
  testHysteresisAndLongDistance();
  testLockout();
  testTenLaps();
  testResetEveryState();
  testErrors();
  testStartupFailure();
  testWideTimestampsAndPrecision();
  std::printf("PASS: 8 scenarios, %u checks\n", checks);
}
