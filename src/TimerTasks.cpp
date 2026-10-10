#include "TimerTasks.h"
#include "MeasurementFlow.h"
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
void printSeconds(std::uint64_t us) {
  const auto hundredths = us / 10000;
  Serial.printf("%llu.%02u", static_cast<unsigned long long>(hundredths / 100),
                static_cast<unsigned>(hundredths % 100));
}
void printDiagnostic(const TimerContext& context) {
  const auto elapsed = context.state == State::Running
      ? SensorReader::nowUs() - context.startUs : context.frozenElapsedUs;
  Serial.printf("STATE=%s SESSION=%llu LAP=%u/10 CAL=%u/10 ELAPSED=",
                stateName(context.state), static_cast<unsigned long long>(context.generation),
                static_cast<unsigned>(context.lapCount),
                static_cast<unsigned>(context.calibrationCount));
  printSeconds(elapsed);
  Serial.printf(" s ERROR=%s\n", errorName(context.error));
  for (std::size_t i = 0; i < context.lapCount; ++i) {
    Serial.printf("LAP %u: ", static_cast<unsigned>(i + 1));
    printSeconds(context.laps[i].lapUs);
    Serial.print(" s TOTAL=");
    printSeconds(context.laps[i].elapsedUs);
    Serial.println(" s");
  }
}
void presentationTask(void*) {
  waitForStart();
  TimerContext snapshot;
  bool haveSnapshot = false;
  bool dirty = false;
  bool haveScreen = false;
  State screenState = State::Idle;
  Error screenError = Error::None;
  TickType_t lastSerial = xTaskGetTickCount() - ticks(500);
  for (;;) {
    TimerContext received;
    if (xQueueReceive(displayQueue, &received, ticks(50)) == pdTRUE) {
      snapshot = received;
      haveSnapshot = true;
      dirty = true;
    }
    if (!haveSnapshot) continue;
    if (!haveScreen || screenState != snapshot.state || screenError != snapshot.error) {
      M5.Display.fillScreen(BLACK);
      M5.Display.setCursor(0, 0);
      M5.Display.setTextSize(2);
      M5.Display.println("Lap timer - stage 2");
      M5.Display.println(stateName(snapshot.state));
      M5.Display.println(errorName(snapshot.error));
      M5.Display.println("See serial diagnostics.");
      M5.Display.println(snapshot.state == State::Idle ? "A: Start" : "A: Reset");
      screenState = snapshot.state;
      screenError = snapshot.error;
      haveScreen = true;
    }
    const auto now = xTaskGetTickCount();
    if ((dirty || snapshot.state == State::Running) && now - lastSerial >= ticks(500)) {
      printDiagnostic(snapshot);
      lastSerial = now;
      dirty = false;
    }
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
