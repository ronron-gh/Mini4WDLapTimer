#include "TimerTasks.h"
#include "MeasurementFlow.h"
#include "TimerDisplay.h"
#include "HiddenBenchmark.h"
#include "Benchmark.h"
#include <M5Unified.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <type_traits>

namespace lap_timer {
namespace {
constexpr std::uint32_t kACommand = 1;
constexpr std::uint32_t kCCommand = 2;
constexpr EventBits_t kStart = 1;
QueueHandle_t displayQueue = nullptr;
QueueHandle_t inputQueue = nullptr;
QueueHandle_t ackQueue = nullptr;
EventGroupHandle_t startGate = nullptr;
TaskHandle_t measurementHandle = nullptr;
TaskHandle_t inputHandle = nullptr;
TaskHandle_t presentationHandle = nullptr;
SensorReader* sensorReader = nullptr;
LapTimer timer;
HiddenBenchmark hidden;
Benchmark benchmark;
struct UiSnapshot {
  TimerContext timer;
  HiddenMode mode;
  std::uint64_t run;
};
struct InputSnapshot {
  State state;
  std::uint64_t generation;
  HiddenMode mode;
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
  const UiSnapshot ui{context, hidden.mode(), hidden.run()};
  xQueueOverwrite(displayQueue, &ui);
  const InputSnapshot input{context.state, context.generation, hidden.mode()};
  xQueueOverwrite(inputQueue, &input);
}
void measurementTask(void*) {
  waitForStart();
  publish();
  for (;;) {
    const auto beforeMode = hidden.mode();
    const auto beforeRun = hidden.run();
    auto bits = commands();
    if (hidden.mode() == HiddenMode::Normal) {
      if (bits & kACommand) timer.onAButton();
      hidden.request(bits & kACommand, bits & kCCommand,
                     timer.context().state == State::Idle, SensorReader::nowUs());
    } else {
      hidden.request(bits & kACommand, false, false, SensorReader::nowUs());
    }
    hidden.poll(SensorReader::nowUs());
    BenchmarkAck ack;
    while (xQueueReceive(ackQueue, &ack, 0) == pdTRUE)
      hidden.acknowledge(ack, SensorReader::nowUs());
    if (hidden.mode() == HiddenMode::Capturing && beforeMode != HiddenMode::Capturing)
      benchmark.begin();
    if (hidden.mode() == HiddenMode::Results && hidden.preparationFailed() &&
        beforeMode != HiddenMode::Results) benchmark.failPreparation();
    if (beforeMode != hidden.mode() || beforeRun != hidden.run() || (bits & kACommand))
      publish();

    if (!sensorReader->ready() && hidden.mode() != HiddenMode::Capturing) {
      vTaskDelay(ticks(50));
      continue;
    }
    const auto sample = sensorReader->collectSample();
    bits = commands();
    if (hidden.mode() == HiddenMode::Normal) {
      const auto before = timer.context();
      applyCompletedSample(timer, sample, (bits & kACommand) != 0);
      const auto mode = hidden.mode();
      hidden.request(bits & kACommand, bits & kCCommand,
                     timer.context().state == State::Idle, sample.readEndUs);
      const auto& after = timer.context();
      if (before.state != after.state || before.generation != after.generation ||
          before.lapCount != after.lapCount || before.error != after.error ||
          before.calibrationCount != after.calibrationCount || mode != hidden.mode())
        publish();
    } else {
      if (benchmark.processSample(hidden, sample, (bits & kACommand) != 0)) publish();
    }
    vTaskDelay(1);
  }
}
void inputTask(void*) {
  waitForStart();
  auto wake = xTaskGetTickCount();
  InputSnapshot state{State::Idle, 0, HiddenMode::Normal};
  HiddenHold hold;
  for (;;) {
    xQueueReceive(inputQueue, &state, 0);
    M5.update();
    const bool a = M5.BtnA.wasPressed();
    const bool c = hold.update(M5.BtnC.isPressed(),
        !a && state.mode == HiddenMode::Normal && state.state == State::Idle,
        state.generation, SensorReader::nowUs());
    if (a) xTaskNotify(measurementHandle, kACommand, eSetBits);
    else if (c) xTaskNotify(measurementHandle, kCCommand, eSetBits);
    vTaskDelayUntil(&wake, ticks(10));
  }
}
void presentationTask(void*) {
  waitForStart();
  UiSnapshot snapshot;
  TimerDisplay display;
  BenchmarkAck pending;
  bool ackPending = false;
  std::uint64_t quietRun = 0;
  std::uint64_t outputRun = 0;
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
    UiSnapshot received;
    // Sleep until the next LCD deadline even when snapshots arrive rapidly.
    const auto current = xTaskGetTickCount();
    const auto age = current - lastLcd;
    if (age < ticks(50)) vTaskDelay(ticks(50) - age);
    if (xQueueReceive(displayQueue, &received, 0) == pdTRUE) {
      snapshot = received;
      haveSnapshot = true;
    }
    if (!haveSnapshot) { vTaskDelay(ticks(50)); continue; }
    if (ackPending && xQueueSend(ackQueue, &pending, 0) == pdTRUE) ackPending = false;
    if (snapshot.mode != HiddenMode::Normal) {
      if (snapshot.mode == HiddenMode::Preparing && quietRun != snapshot.run) {
        display.renderBenchmarkPreparing();
        Serial.flush();
        quietRun = snapshot.run;
        pending.kind = BenchmarkAckKind::Quiet;
        pending.run = snapshot.run;
        ackPending = true;
      } else if (snapshot.mode == HiddenMode::Results && outputRun != snapshot.run) {
        const auto summary = benchmark.summary();
        benchmark.printResults(Serial, *sensorReader);
        Serial.flush();
        display.renderBenchmarkResult(summary);
        outputRun = snapshot.run;
        pending.kind = BenchmarkAckKind::OutputDone;
        pending.run = snapshot.run;
        ackPending = true;
      }
      // Capturing is deliberately silent. The buffer is immutable until OutputDone.
      lastLcd = xTaskGetTickCount();
      continue;
    }
    const auto model = makeDisplayModel(snapshot.timer, SensorReader::nowUs());
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
  if (ackQueue) vQueueDelete(ackQueue);
  if (startGate) vEventGroupDelete(startGate);
  measurementHandle = inputHandle = presentationHandle = nullptr;
  displayQueue = inputQueue = ackQueue = nullptr;
  startGate = nullptr;
}
}  // namespace
bool startTimerTasks(SensorReader& reader) {
  if (startGate != nullptr) return false;
  sensorReader = &reader;
  timer.initialize(reader.startupError());
  hidden = HiddenBenchmark{};
  startGate = xEventGroupCreate();
  displayQueue = xQueueCreate(1, sizeof(UiSnapshot));
  inputQueue = xQueueCreate(1, sizeof(InputSnapshot));
  ackQueue = xQueueCreate(2, sizeof(BenchmarkAck));
  if (!startGate || !displayQueue || !inputQueue || !ackQueue) {
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
