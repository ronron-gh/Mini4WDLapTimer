#include "DisplayModel.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace lap_timer;
static unsigned checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
  std::fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); std::exit(1); \
} } while (false)
static void expectTime(std::uint64_t us, const char* expected) {
  char buffer[28];
  CHECK(formatSeconds(us, buffer, sizeof(buffer)));
  CHECK(std::strcmp(buffer, expected) == 0);
}
int main() {
  expectTime(0, "0.00");
  expectTime(9999, "0.00");
  expectTime(10000, "0.01");
  expectTime(999999, "0.99");
  expectTime(1000000, "1.00");
  expectTime(4294967296ULL, "4294.96");
  expectTime(UINT64_MAX, "18446744073709.55");
  char small[4] = "abc";
  CHECK(!formatSeconds(0, small, sizeof(small)));
  CHECK(small[0] == '\0');
  CHECK(!formatSeconds(0, nullptr, 10));
  CHECK(!formatSeconds(0, small, 0));

  TimerContext context;
  auto idle = makeDisplayModel(context, 123456);
  CHECK(idle.elapsedCs == 0);
  CHECK(idle.lapCount == 0);
  CHECK(idle.rows[0].visible && !idle.rows[0].confirmed);
  CHECK(std::strcmp(actionLabel(idle.state), "スタート") == 0);
  for (unsigned i = 1; i < 10; ++i) CHECK(!idle.rows[i].visible);
  context.state = State::Calibrating;
  context.calibrationCount = 6;
  auto calibration = makeDisplayModel(context, 999999);
  CHECK(calibration.rows[0].visible && !calibration.rows[0].confirmed);
  CHECK(calibration.calibrationCount == 6);
  CHECK(std::strcmp(actionName(calibration.state), "RESET") == 0);
  CHECK(!sameDisplayModel(idle, calibration));
  context.state = State::Armed;
  auto armed = makeDisplayModel(context, 999999);
  for (const auto& row : armed.rows) CHECK(!row.visible);
  CHECK(armed.elapsedCs == 0);
  CHECK(std::strcmp(stateLabel(armed.state), "スタート待ち") == 0);

  context.state = State::Running;
  context.startUs = 0x100000000ULL;
  context.lapCount = 1;
  context.laps[0].confirmed = true;
  context.laps[0].lapUs = 1234567;
  context.laps[0].elapsedUs = 1234567;
  auto running = makeDisplayModel(context, context.startUs + 2345678);
  CHECK(running.elapsedCs == 234);
  CHECK(running.rows[0].lapCs == 123);
  CHECK(running.rows[0].elapsedCs == 123);
  CHECK(running.rows[0].confirmed);
  CHECK(!running.rows[1].visible);
  auto same = makeDisplayModel(context, context.startUs + 2349999);
  CHECK(sameDisplayModel(running, same));
  auto changed = makeDisplayModel(context, context.startUs + 2350000);
  CHECK(!sameDisplayModel(running, changed));
  CHECK(makeDisplayModel(context, context.startUs - 1).elapsedCs == 0);

  context.state = State::Complete;
  context.lapCount = 10;
  context.frozenElapsedUs = 120790000;
  for (unsigned i = 0; i < 10; ++i) {
    context.laps[i].confirmed = true;
    context.laps[i].lapUs = 12000000 + i;
    context.laps[i].elapsedUs = 12000000ULL * (i + 1);
  }
  auto complete = makeDisplayModel(context, UINT64_MAX);
  CHECK(complete.elapsedCs == 12079);
  CHECK(complete.lapCount == 10);
  for (const auto& row : complete.rows) CHECK(row.visible && row.confirmed);
  CHECK(sameDisplayModel(complete, makeDisplayModel(context, 0)));
  context.state = State::Error;
  context.error = Error::SensorTimeout;
  auto error = makeDisplayModel(context, UINT64_MAX);
  CHECK(error.elapsedCs == complete.elapsedCs);
  CHECK(error.rows[9].confirmed);
  CHECK(std::strcmp(errorName(error.error), "SENSOR_TIMEOUT") == 0);
  CHECK(std::strcmp(actionLabel(error.state), "リセット") == 0);
  CHECK(!sameDisplayModel(complete, error));

  context = TimerContext{};
  context.generation = 2;
  auto reset = makeDisplayModel(context, 999999);
  CHECK(reset.rows[0].visible && !reset.rows[0].confirmed);
  CHECK(reset.elapsedCs == 0 && reset.lapCount == 0);
  CHECK(!reset.rows[9].visible);
  CHECK(!sameDisplayModel(idle, reset));
  auto altered = reset;
  altered.rows[1].visible = true;
  CHECK(!sameDisplayModel(reset, altered));
  altered = reset;
  altered.calibrationCount = 1;
  CHECK(!sameDisplayModel(reset, altered));
  char cs[28];
  CHECK(formatCentiseconds(UINT64_MAX, cs, sizeof(cs)));
  CHECK(std::strcmp(cs, "184467440737095516.15") == 0);
  TimerContext baselineContext;
  baselineContext.state = State::Calibrating;
  baselineContext.calibrationCount = 9;
  baselineContext.baselineSumMm = 2700;
  auto pendingBaseline = makeDisplayModel(baselineContext, 0);
  CHECK(!pendingBaseline.baselineReady);
  char distance[20];
  CHECK(!formatBaselineDistance(pendingBaseline, distance, sizeof(distance)));
  CHECK(distance[0] == '\0');
  baselineContext.calibrationCount = 10;
  baselineContext.baselineSumMm = 3000;
  baselineContext.state = State::Armed;
  auto integerBaseline = makeDisplayModel(baselineContext, 0);
  CHECK(integerBaseline.baselineReady);
  CHECK(formatBaselineDistance(integerBaseline, distance, sizeof(distance)));
  CHECK(std::strcmp(distance, "300.0") == 0);
  baselineContext.baselineSumMm = 3005;
  baselineContext.state = State::Running;
  auto fractionalBaseline = makeDisplayModel(baselineContext, 0);
  CHECK(fractionalBaseline.baselineReady);
  CHECK(formatBaselineDistance(fractionalBaseline, distance, sizeof(distance)));
  CHECK(std::strcmp(distance, "300.5") == 0);
  CHECK(!sameDisplayModel(integerBaseline, fractionalBaseline));
  auto alteredBaseline = fractionalBaseline;
  alteredBaseline.baselineSumMm = 3006;
  CHECK(!sameDisplayModel(fractionalBaseline, alteredBaseline));
  baselineContext.state = State::Error;
  baselineContext.calibrationCount = 9;
  CHECK(!makeDisplayModel(baselineContext, 0).baselineReady);
  baselineContext = TimerContext{};
  baselineContext.generation = 3;
  auto resetBaseline = makeDisplayModel(baselineContext, 0);
  CHECK(!resetBaseline.baselineReady && resetBaseline.baselineSumMm == 0);
  char tooSmall[5];
  CHECK(!formatBaselineDistance(fractionalBaseline, tooSmall, sizeof(tooSmall)));
  CHECK(tooSmall[0] == '\0');
  std::printf("PASS: display model, %u checks\n", checks);
}
