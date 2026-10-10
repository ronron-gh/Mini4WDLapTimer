#include "BuildMode.h"
#include "TaskRuntime.h"
#include "Benchmark.h"
#include "TimerDisplay.h"
#include <Arduino.h>
static_assert(LAP_TIMER_BENCHMARK_MODE == 1, "Benchmark task source selected for normal build");
namespace lap_timer {
namespace {
BenchmarkSession session;
Benchmark benchmark;
struct Snapshot { BenchmarkPhase phase; std::uint64_t run; };
void publish() {
  const Snapshot snapshot{session.mode(), session.run()};
  xQueueOverwrite(runtime.displayQueue, &snapshot);
}
}
bool configureModeTasks() {
  session = BenchmarkSession{};
  runtime.displayQueue = xQueueCreate(1, sizeof(Snapshot));
  runtime.ackQueue = xQueueCreate(2, sizeof(BenchmarkAck));
  return runtime.displayQueue && runtime.ackQueue;
}
void measurementTask(void*) {
  waitForTaskStart();
  session.start(SensorReader::nowUs());
  publish();
  for (;;) {
    const auto phase = session.mode();
    const auto run = session.run();
    session.requestAgain(takeACommand(), SensorReader::nowUs());
    session.poll(SensorReader::nowUs());
    BenchmarkAck ack;
    while (xQueueReceive(runtime.ackQueue, &ack, 0) == pdTRUE)
      session.acknowledge(ack, SensorReader::nowUs());
    if (session.mode() == BenchmarkPhase::Capturing && phase != BenchmarkPhase::Capturing)
      benchmark.begin();
    if (session.mode() == BenchmarkPhase::Results && session.preparationFailed() &&
        phase != BenchmarkPhase::Results) benchmark.failPreparation();
    if (phase != session.mode() || run != session.run()) publish();
    if (!runtime.reader->ready() && session.mode() != BenchmarkPhase::Capturing) {
      vTaskDelay(taskTicks(50));
      continue;
    }
    const auto sample = runtime.reader->collectSample();
    if (benchmark.processSample(session, sample, takeACommand())) publish();
    vTaskDelay(1);
  }
}
void presentationTask(void*) {
  waitForTaskStart();
  TimerDisplay display;
  Snapshot snapshot{};
  BenchmarkAck pending;
  bool haveSnapshot = false;
  bool ackPending = false;
  std::uint64_t quietRun = 0;
  std::uint64_t outputRun = 0;
  for (;;) {
    Snapshot received;
    if (xQueueReceive(runtime.displayQueue, &received, 0) == pdTRUE) {
      snapshot = received;
      haveSnapshot = true;
    }
    if (ackPending && xQueueSend(runtime.ackQueue, &pending, 0) == pdTRUE)
      ackPending = false;
    if (haveSnapshot && snapshot.phase == BenchmarkPhase::Preparing &&
        quietRun != snapshot.run) {
      display.renderBenchmarkPreparing();
      Serial.flush();
      quietRun = snapshot.run;
      pending.kind = BenchmarkAckKind::Quiet;
      pending.run = snapshot.run;
      ackPending = true;
    } else if (haveSnapshot && snapshot.phase == BenchmarkPhase::Results &&
               outputRun != snapshot.run) {
      const auto summary = benchmark.summary();
      benchmark.printResults(Serial, *runtime.reader);
      Serial.flush();
      display.renderBenchmarkResult(summary);
      outputRun = snapshot.run;
      pending.kind = BenchmarkAckKind::OutputDone;
      pending.run = snapshot.run;
      ackPending = true;
    }
    vTaskDelay(taskTicks(50));
  }
}
}  // namespace lap_timer
