#include "TimerTasks.h"
#include "TaskRuntime.h"
#include <M5Unified.h>
namespace lap_timer {
TaskRuntime runtime;
namespace {
constexpr EventBits_t kStart = 1;
constexpr std::uint32_t kACommand = 1;
void inputTask(void*) {
  waitForTaskStart();
  auto wake = xTaskGetTickCount();
  for (;;) {
    M5.update();
    if (M5.BtnA.wasPressed()) xTaskNotify(runtime.measurement, kACommand, eSetBits);
    vTaskDelayUntil(&wake, taskTicks(10));
  }
}
void releaseResources() {
  if (runtime.measurement) vTaskDelete(runtime.measurement);
  if (runtime.input) vTaskDelete(runtime.input);
  if (runtime.presentation) vTaskDelete(runtime.presentation);
  if (runtime.displayQueue) vQueueDelete(runtime.displayQueue);
  if (runtime.ackQueue) vQueueDelete(runtime.ackQueue);
  if (runtime.startGate) vEventGroupDelete(runtime.startGate);
  runtime = TaskRuntime{};
}
}
TickType_t taskTicks(std::uint32_t ms) {
  const auto result = pdMS_TO_TICKS(ms);
  return result ? result : 1;
}
void waitForTaskStart() {
  xEventGroupWaitBits(runtime.startGate, kStart, pdFALSE, pdTRUE, portMAX_DELAY);
}
bool takeACommand() {
  std::uint32_t bits = 0;
  xTaskNotifyWait(0, UINT32_MAX, &bits, 0);
  return (bits & kACommand) != 0;
}
bool startTimerTasks(SensorReader& reader) {
  if (runtime.startGate) return false;
  runtime.reader = &reader;
  runtime.startGate = xEventGroupCreate();
  if (!runtime.startGate || !configureModeTasks()) {
    releaseResources();
    return false;
  }
  if (xTaskCreatePinnedToCore(presentationTask, "presentation", 8192, nullptr, 1,
                             &runtime.presentation, 0) != pdPASS ||
      xTaskCreatePinnedToCore(inputTask, "input", 3072, nullptr, 2,
                             &runtime.input, 0) != pdPASS ||
      xTaskCreatePinnedToCore(measurementTask, "measurement", 6144, nullptr, 3,
                             &runtime.measurement, 1) != pdPASS) {
    releaseResources();
    return false;
  }
  xEventGroupSetBits(runtime.startGate, kStart);
  return true;
}
}  // namespace lap_timer
