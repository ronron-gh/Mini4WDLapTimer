#pragma once
#include "DisplayModel.h"
#include "BenchmarkData.h"
#include <Stream.h>
namespace lap_timer {
class TimerDisplay {
 public:
  void renderBenchmarkPreparing();
  void renderBenchmarkResult(const BenchmarkSummary& summary);
  void renderLcd(const DisplayModel& model);
  void outputBaseline(Stream& output, const DisplayModel& model) const;
  void outputSerial(Stream& output, const DisplayModel& model) const;
 private:
  bool drawn_ = false;
  DisplayModel previous_{};
};
}  // namespace lap_timer
