#include <Arduino.h>
#include <M5Unified.h>
#include <VL53L0X.h>
#include <esp_timer.h>

namespace {
constexpr size_t kSampleCount = 50;
constexpr uint16_t kMeasurementTimeoutMs = 1000;
constexpr uint16_t kI2cTimeoutMs = 20;
constexpr uint32_t kI2cFrequencyHz = 400000;
// Set to 0 to compare with the library's default timing budget.
constexpr uint32_t kTimingBudgetUs = 20000;

enum class SampleStatus : uint8_t {
  Ok, Timeout, I2cError, ReadError, InitError, ConfigError, NotRun
};

struct Sample {
  uint64_t readStartUs;
  uint64_t readEndUs;
  uint16_t distanceMm;
  SampleStatus status;
};

// Fixed RAM storage; no LCD or serial output while collecting samples.
Sample samples[kSampleCount];
VL53L0X sensor;
bool busReady = false;
uint32_t actualTimingBudgetUs = 0;
const char* batchError = nullptr;

uint64_t nowUs() {
  // Microseconds since boot, not calendar time.
  return static_cast<uint64_t>(esp_timer_get_time());
}

void collectSample(Sample& sample) {
  sample.readStartUs = nowUs();
  sample.distanceMm = sensor.readRangeContinuousMillimeters();
  sample.readEndUs = nowUs();
  if (sensor.timeoutOccurred()) {
    sample.status = SampleStatus::Timeout;
  } else if (sensor.last_status != 0) {
    sample.status = SampleStatus::I2cError;
  } else if (sample.distanceMm == UINT16_MAX) {
    sample.status = SampleStatus::ReadError;
  } else {
    // This API does not expose the sensor's range quality status.
    sample.status = SampleStatus::Ok;
  }
}

const char* statusName(SampleStatus status) {
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

struct Statistics {
  uint64_t sum = 0;
  uint64_t minimum = UINT64_MAX;
  uint64_t maximum = 0;
  size_t count = 0;

  void add(uint64_t value) {
    sum += value;
    if (value < minimum) minimum = value;
    if (value > maximum) maximum = value;
    ++count;
  }

  void print(const char* name) const {
    if (count == 0) {
      Serial.printf("# %s: count=0\n", name);
      return;
    }
    Serial.printf("# %s: count=%u,min_us=%llu,mean_us=%.2f,max_us=%llu\n",
                  name, static_cast<unsigned>(count),
                  static_cast<unsigned long long>(minimum),
                  static_cast<double>(sum) / count,
                  static_cast<unsigned long long>(maximum));
  }
};

void printResults() {
  Statistics durations;
  Statistics intervals;
  size_t successful = 0;
  Serial.printf("# mode=continuous_back_to_back,i2c_hz=%lu,requested_budget_us=%lu,actual_budget_us=%lu\n",
                static_cast<unsigned long>(kI2cFrequencyHz),
                static_cast<unsigned long>(kTimingBudgetUs),
                static_cast<unsigned long>(actualTimingBudgetUs));
  Serial.println("# timestamps: microseconds since boot; read_end = API return time");
  Serial.println("# read_duration is API wait/read time, not sensor integration time");
  Serial.println("# OK = no detected acquisition error; range quality is unavailable");
  if (batchError != nullptr) Serial.printf("# error: %s\n", batchError);
  Serial.println("index,read_start_us,read_end_us,read_duration_us,interval_us,distance_mm,status");
  for (size_t i = 0; i < kSampleCount; ++i) {
    const Sample& sample = samples[i];
    Serial.printf("%u,", static_cast<unsigned>(i + 1));
    if (sample.status == SampleStatus::NotRun) {
      Serial.println(",,,,,NOT_RUN");
      continue;
    }
    Serial.printf("%llu,%llu,",
                  static_cast<unsigned long long>(sample.readStartUs),
                  static_cast<unsigned long long>(sample.readEndUs));
    if (sample.status == SampleStatus::Ok) {
      const uint64_t duration = sample.readEndUs - sample.readStartUs;
      durations.add(duration);
      ++successful;
      Serial.printf("%llu,", static_cast<unsigned long long>(duration));
      if (i > 0 && samples[i - 1].status == SampleStatus::Ok) {
        const uint64_t interval = sample.readEndUs - samples[i - 1].readEndUs;
        intervals.add(interval);
        Serial.printf("%llu,", static_cast<unsigned long long>(interval));
      } else {
        Serial.print(',');
      }
      Serial.printf("%u,OK\n", static_cast<unsigned>(sample.distanceMm));
    } else {
      Serial.printf(",,,%s\n", statusName(sample.status));
    }
  }
  Serial.printf("# samples=%u,acquired=%u\n", static_cast<unsigned>(kSampleCount),
                static_cast<unsigned>(successful));
  durations.print("read_duration");
  // First sample has no interval and does not affect the period summary.
  intervals.print("interval");
  Serial.println("# Press button A to run again.");
}

SampleStatus startupStatus = SampleStatus::NotRun;
const char* startupError = nullptr;

// Called once from setup(); continuous ranging remains active between batches.
void initializeSensor() {
  startupStatus = SampleStatus::InitError;
  startupError = "Sensor initialization failed";
  if (!busReady || !sensor.init() || sensor.last_status != 0) return;

  startupStatus = SampleStatus::ConfigError;
  startupError = "Requested measurement timing budget rejected";
  if (kTimingBudgetUs != 0 &&
      (!sensor.setMeasurementTimingBudget(kTimingBudgetUs) ||
       sensor.last_status != 0)) return;

  startupStatus = SampleStatus::I2cError;
  startupError = "Could not read timing budget";
  actualTimingBudgetUs = sensor.getMeasurementTimingBudget();
  if (sensor.last_status != 0) return;

  startupError = "Could not start continuous ranging";
  sensor.timeoutOccurred();
  sensor.startContinuous();
  if (sensor.last_status != 0) return;

  startupStatus = SampleStatus::Ok;
  startupError = nullptr;
}

void runBenchmark() {
  for (Sample& sample : samples) {
    sample = {};
    sample.status = SampleStatus::NotRun;
  }
  batchError = startupError;
  M5.Display.fillScreen(BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.println("Collecting 50 samples...");
  Serial.flush();

  if (startupStatus != SampleStatus::Ok) {
    samples[0].readStartUs = nowUs();
    samples[0].readEndUs = samples[0].readStartUs;
    samples[0].status = startupStatus;
  } else {
    sensor.timeoutOccurred();  // Clear the previous batch's timeout flag.
    // Discard a pending reading from the idle/logging period. Its completion
    // time is unknown; the following 50 readings are the measured batch.
    Sample pending = {};
    collectSample(pending);
    if (pending.status != SampleStatus::Ok) {
      samples[0] = pending;
      batchError = "Could not discard pending reading";
    } else {
      for (Sample& sample : samples) {
        collectSample(sample);
        if (sample.status != SampleStatus::Ok) break;
      }
    }
  }
  printResults();
  M5.Display.println("Done. See serial log.");
  M5.Display.println("Press A to run again.");
}
}  // namespace

void setup() {
  M5.begin();
  busReady = Wire.begin(21, 22);
  if (busReady) busReady = Wire.setClock(kI2cFrequencyHz);
  Wire.setTimeOut(kI2cTimeoutMs);
  sensor.setBus(&Wire);
  sensor.setTimeout(kMeasurementTimeoutMs);
  Serial.begin(115200);
  initializeSensor();
  runBenchmark();
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) runBenchmark();
  delay(10);  // Idle only; acquisition has no LCD, serial or fixed delays.
}
