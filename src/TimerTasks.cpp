#include "TimerTasks.h"
#include "MeasurementFlow.h"
#include "TimerDisplay.h"
#include <M5Unified.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <type_traits>

namespace lap_timer {
namespace {
constexpr std::uint32_t kACommand = 1;
constexpr EventBits_t kStart = 1;
QueueHandle_t displayQueue = nullptr;
QueueHandle_t inputQueue = nullptr;
EventGroupHandle_t startGate = nullptr;
TaskHandle_t measurementHandle = nullptr;
TaskHandle_t inputHandle = nullptr;
TaskHandle_t presentationHandle = nullptr;
SensorReader* sensorReader = nullptr;
LapTimer timer;
struct InputSnapshot {
  State state;
  std::uint64_t generation;
};
static_assert(std::is_trivially_copyable<TimerContext>::value, "Queue items must be value copies");

TickType_t ticks(std::uint32_t ms) {
  const auto result = pdMS_TO_TICKS(ms);
  return result == 0 ? 1 : result;
}
void waitForStart() {
  xEventGroupWaitBits(startGate, kStart, pdFALSE, pdTRUE, portMAX_DELAY);
}
std::uint32_t commands() {
  std::uint32_t bits = 0;
  xTaskNotifyWait(0, UINT32_MAX, &bits, 0);
  return bits;
}
void publish() {
  const auto& context = timer.context();
  xQueueOverwrite(displayQueue, &context);
  const InputSnapshot input{context.state, context.generation};
  xQueueOverwrite(inputQueue, &input);
}
void measurementTask(void*) {
  waitForStart();
  publish();
  for (;;) {
    if (commands() & kACommand) {
      timer.onAButton();
      publish();
    }
    if (!sensorReader->ready()) {
      vTaskDelay(ticks(50));
      continue;
    }
    const auto sample = sensorReader->collectSample();
    const bool cancelled = (commands() & kACommand) != 0;
    const auto before = timer.context();
    applyCompletedSample(timer, sample, cancelled);
    const auto& after = timer.context();
    if (before.state != after.state || before.generation != after.generation ||
        before.lapCount != after.lapCount || before.error != after.error ||
        before.calibrationCount != after.calibrationCount) publish();
    vTaskDelay(1);
  }
}
void inputTask(void*) {
  waitForStart();
  auto wake = xTaskGetTickCount();
  InputSnapshot state{State::Idle, 0};
  for (;;) {
    xQueueReceive(inputQueue, &state, 0);
    M5.update();
    if (M5.BtnA.wasPressed()) xTaskNotify(measurementHandle, kACommand, eSetBits);
    vTaskDelayUntil(&wake, ticks(10));
  }
}
void presentationTask(void*) {
  waitForStart();
  TimerContext snapshot;
  TimerDisplay display;
  /* Debug lap output: uncomment these declarations and the block below.
  DisplayModel lastSerialModel;
  bool serialSent = false;
  */
  bool baselineLogged = false;
  std::uint64_t loggedGeneration = 0;
  bool haveSnapshot = false;
  auto lastLcd = xTaskGetTickCount() - ticks(50);
  auto lastSerial = xTaskGetTickCount() - ticks(500);
  for (;;) {
    TimerContext received;
    // Sleep until the next LCD deadline even when snapshots arrive rapidly.
    const auto current = xTaskGetTickCount();
    const auto age = current - lastLcd;
    if (age < ticks(50)) vTaskDelay(ticks(50) - age);
    if (xQueueReceive(displayQueue, &received, 0) == pdTRUE) {
      snapshot = received;
      haveSnapshot = true;
    }
    if (!haveSnapshot) { vTaskDelay(ticks(50)); continue; }
    const auto model = makeDisplayModel(snapshot, SensorReader::nowUs());
    display.renderLcd(model);
    // Start spacing after rendering to avoid catch-up bursts after slow output.
    lastLcd = xTaskGetTickCount();
    const auto now = xTaskGetTickCount();
    if (model.baselineReady && (!baselineLogged || loggedGeneration != model.generation) &&
        now - lastSerial >= ticks(500)) {
      display.outputBaseline(Serial, model);
      loggedGeneration = model.generation;
      baselineLogged = true;
      lastSerial = xTaskGetTickCount();
    }
    /* Debug lap output: retained for occasional diagnostics.
    if ((!serialSent || !sameDisplayModel(model, lastSerialModel)) &&
        xTaskGetTickCount() - lastSerial >= ticks(500)) {
      display.outputSerial(Serial, model);
      lastSerialModel = model;
      serialSent = true;
      lastSerial = xTaskGetTickCount();
    }
    */
  }
}
void releaseResources() {
  if (measurementHandle) vTaskDelete(measurementHandle);
  if (inputHandle) vTaskDelete(inputHandle);
  if (presentationHandle) vTaskDelete(presentationHandle);
  if (displayQueue) vQueueDelete(displayQueue);
  if (inputQueue) vQueueDelete(inputQueue);
  if (startGate) vEventGroupDelete(startGate);
  measurementHandle = inputHandle = presentationHandle = nullptr;
  displayQueue = inputQueue = nullptr;
  startGate = nullptr;
}
}  // namespace
bool startTimerTasks(SensorReader& reader) {
  if (startGate != nullptr) return false;
  sensorReader = &reader;
  timer.initialize(reader.startupError());
  startGate = xEventGroupCreate();
  displayQueue = xQueueCreate(1, sizeof(TimerContext));
  inputQueue = xQueueCreate(1, sizeof(InputSnapshot));
  if (!startGate || !displayQueue || !inputQueue) {
    releaseResources();
    return false;
  }
  if (xTaskCreatePinnedToCore(presentationTask, "presentation", 8192, nullptr, 1,
                            &presentationHandle, 0) != pdPASS ||
      xTaskCreatePinnedToCore(inputTask, "input", 3072, nullptr, 2,
                            &inputHandle, 0) != pdPASS ||
      xTaskCreatePinnedToCore(measurementTask, "measurement", 6144, nullptr, 3,
                            &measurementHandle, 1) != pdPASS) {
    releaseResources();
    return false;
  }
  xEventGroupSetBits(startGate, kStart);
  return true;
}
}  // namespace lap_timer
