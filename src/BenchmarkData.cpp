#include "BenchmarkData.h"
namespace lap_timer {
void BenchmarkStatistics::add(std::uint64_t value) {
  sum += value;
  if (value < minimum) minimum = value;
  if (value > maximum) maximum = value;
  ++count;
}
void BenchmarkData::begin() {
  samples_ = {};
  count_ = 0;
  discard_ = true;
  done_ = false;
  preparationFailed_ = false;
  error_ = SampleStatus::Ok;
}
void BenchmarkData::failPreparation() {
  begin();
  preparationFailed_ = true;
  done_ = true;
}
void BenchmarkData::consume(const SensorSample& sample) {
  if (done_) return;
  if (discard_) {
    discard_ = false;
    if (sample.status == SampleStatus::Ok) return;
  }
  samples_[count_++] = sample;
  if (sample.status != SampleStatus::Ok) {
    error_ = sample.status;
    done_ = true;
  } else if (count_ == samples_.size()) {
    done_ = true;
  }
}
BenchmarkSummary BenchmarkData::summary() const {
  BenchmarkSummary result;
  result.attempted = count_;
  result.error = error_;
  result.preparationFailed = preparationFailed_;
  for (std::size_t i = 0; i < samples_.size(); ++i) {
    const auto& sample = samples_[i];
    if (sample.status != SampleStatus::Ok) continue;
    ++result.successful;
    result.durations.add(sample.readEndUs - sample.readStartUs);
    if (i > 0 && samples_[i-1].status == SampleStatus::Ok)
      result.intervals.add(sample.readEndUs - samples_[i-1].readEndUs);
  }
  return result;
}
}  // namespace lap_timer
