#include "Benchmark.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace lap_timer {
constexpr std::size_t kSampleCount = 50;
using Sample = SensorSample;
void Benchmark::capture(SensorReader& reader) {
  begin();
  while (!done()) {
    consume(reader.collectSample());
    vTaskDelay(1);
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

  void print(Stream& output, const char* name) const {
    if (count == 0) {
      output.printf("# %s: count=0\n", name);
      return;
    }
    output.printf("# %s: count=%u,min_us=%llu,mean_us=%.2f,max_us=%llu\n",
                  name, static_cast<unsigned>(count),
                  static_cast<unsigned long long>(minimum),
                  static_cast<double>(sum) / count,
                  static_cast<unsigned long long>(maximum));
  }
};

void Benchmark::printResults(Stream& output, const SensorReader& reader) const {
  const auto& samples_ = samples();
  const auto result = summary();
  const auto batchError_ = result.error;
  Statistics durations;
  Statistics intervals;
  size_t successful = 0;
  output.printf("# mode=continuous_back_to_back,i2c_hz=%lu,requested_budget_us=%lu,actual_budget_us=%lu\n",
                static_cast<unsigned long>(kI2cFrequencyHz),
                static_cast<unsigned long>(kTimingBudgetUs),
                static_cast<unsigned long>(reader.actualTimingBudgetUs()));
  output.println("# timestamps: microseconds since boot; read_end = API return time");
  output.println("# read_duration is API wait/read time, not sensor integration time");
  output.println("# OK = no detected acquisition error; range quality is unavailable");
  output.println("# task_yield_ticks=1; conditions differ from the original single loop");
  if (result.preparationFailed) output.println("# error: BENCH_PREP_TIMEOUT");
  if (batchError_ != SampleStatus::Ok) output.printf("# error: %s\n", sampleStatusName(batchError_));
  output.println("index,read_start_us,read_end_us,read_duration_us,interval_us,distance_mm,status");
  for (size_t i = 0; i < kSampleCount; ++i) {
    const Sample& sample = samples_[i];
    output.printf("%u,", static_cast<unsigned>(i + 1));
    if (sample.status == SampleStatus::NotRun) {
      output.println(",,,,,NOT_RUN");
      continue;
    }
    output.printf("%llu,%llu,",
                  static_cast<unsigned long long>(sample.readStartUs),
                  static_cast<unsigned long long>(sample.readEndUs));
    if (sample.status == SampleStatus::Ok) {
      const uint64_t duration = sample.readEndUs - sample.readStartUs;
      durations.add(duration);
      ++successful;
      output.printf("%llu,", static_cast<unsigned long long>(duration));
      if (i > 0 && samples_[i - 1].status == SampleStatus::Ok) {
        const uint64_t interval = sample.readEndUs - samples_[i - 1].readEndUs;
        intervals.add(interval);
        output.printf("%llu,", static_cast<unsigned long long>(interval));
      } else {
        output.print(',');
      }
      output.printf("%u,OK\n", static_cast<unsigned>(sample.distanceMm));
    } else {
      output.printf(",,,%s\n", sampleStatusName(sample.status));
    }
  }
  output.printf("# samples=%u,acquired=%u\n", static_cast<unsigned>(kSampleCount),
                static_cast<unsigned>(successful));
  durations.print(output, "read_duration");
  // First sample has no interval and does not affect the period summary.
  intervals.print(output, "interval");
  output.println("# A: measure again; reset repeats benchmark; upload normal build to return.");
}

}  // namespace lap_timer
