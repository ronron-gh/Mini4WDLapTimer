#include "BuildMode.h"
#include "TaskRuntime.h"
#include "MeasurementFlow.h"
#include "TimerDisplay.h"
#include <Arduino.h>
#include <type_traits>
static_assert(LAP_TIMER_BENCHMARK_MODE == 0, "Normal task source selected for benchmark build");
namespace lap_timer {
namespace {
LapTimer timer;
void publish() { xQueueOverwrite(runtime.displayQueue, &timer.context()); }
}
bool configureModeTasks() {
  timer.initialize(runtime.reader->startupError());
  static_assert(std::is_trivially_copyable<TimerContext>::value, "Queue value copy required");
  runtime.displayQueue = xQueueCreate(1, sizeof(TimerContext));
  return runtime.displayQueue != nullptr;
}
void measurementTask(void*) {
  waitForTaskStart();
  publish();
  for (;;) {
    if (takeACommand()) { timer.onAButton(); publish(); }
    if (!runtime.reader->ready()) { vTaskDelay(taskTicks(50)); continue; }
    const auto sample = runtime.reader->collectSample();
    const bool cancelled = takeACommand();
    const auto before = timer.context();
    applyCompletedSample(timer, sample, cancelled);
    const auto& after = timer.context();
    if (before.state != after.state || before.generation != after.generation ||
        before.lapCount != after.lapCount || before.error != after.error ||
        before.calibrationCount != after.calibrationCount) publish();
    vTaskDelay(1);
  }
}
void presentationTask(void*) {
  waitForTaskStart();
  TimerContext snapshot;
  TimerDisplay display;
  /* Debug lap output: uncomment declarations and the block below.
  DisplayModel lastSerialModel;
  bool serialSent = false;
  */
  bool haveSnapshot = false;
  bool baselineLogged = false;
  std::uint64_t loggedGeneration = 0;
  auto lastLcd = xTaskGetTickCount() - taskTicks(50);
  auto lastSerial = xTaskGetTickCount() - taskTicks(500);
  for (;;) {
    const auto age = xTaskGetTickCount() - lastLcd;
    if (age < taskTicks(50)) vTaskDelay(taskTicks(50) - age);
    TimerContext received;
    if (xQueueReceive(runtime.displayQueue, &received, 0) == pdTRUE) {
      snapshot = received;
      haveSnapshot = true;
    }
    if (!haveSnapshot) { vTaskDelay(taskTicks(50)); continue; }
    const auto model = makeDisplayModel(snapshot, SensorReader::nowUs());
    display.renderLcd(model);
    lastLcd = xTaskGetTickCount();
    if (model.baselineReady && (!baselineLogged || loggedGeneration != model.generation) &&
        xTaskGetTickCount() - lastSerial >= taskTicks(500)) {
      display.outputBaseline(Serial, model);
      baselineLogged = true;
      loggedGeneration = model.generation;
      lastSerial = xTaskGetTickCount();
    }
    /* Debug lap output: retained for occasional diagnostics.
    if ((!serialSent || !sameDisplayModel(model, lastSerialModel)) &&
        xTaskGetTickCount() - lastSerial >= taskTicks(500)) {
      display.outputSerial(Serial, model);
      lastSerialModel = model;
      serialSent = true;
      lastSerial = xTaskGetTickCount();
    }
    */
  }
}
}  // namespace lap_timer
