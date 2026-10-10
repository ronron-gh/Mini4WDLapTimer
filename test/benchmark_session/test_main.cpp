#include "BenchmarkSession.h"
#include "BenchmarkAcquisition.h"
#include "BenchmarkData.h"
#include <cstdio>
#include <cstdlib>
using namespace lap_timer;
static unsigned checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
  std::fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); std::exit(1); \
} } while (false)
static SensorSample reading(unsigned i, SampleStatus status = SampleStatus::Ok) {
  SensorSample s;
  s.readStartUs = 100000ULL + i * 19000ULL;
  s.readEndUs = s.readStartUs + 18000;
  s.distanceMm = 300;
  s.status = status;
  return s;
}
int main() {
  BenchmarkSession mode;
  mode.start(2);
  CHECK(mode.mode() == BenchmarkPhase::Preparing && mode.run() == 1);
  BenchmarkAck ack;
  ack.kind = BenchmarkAckKind::Quiet; ack.run = 0;
  mode.acknowledge(ack, 3);
  CHECK(mode.mode() == BenchmarkPhase::Preparing);
  mode.requestAgain(true, 4);
  ack.run = 1;
  mode.acknowledge(ack, 5);
  CHECK(mode.mode() == BenchmarkPhase::Capturing);
  mode.requestAgain(true, 6);
  mode.captureDone();
  CHECK(mode.mode() == BenchmarkPhase::Results);
  mode.requestAgain(true, 7);
  CHECK(mode.mode() == BenchmarkPhase::Results);
  ack.kind = BenchmarkAckKind::OutputDone;
  mode.acknowledge(ack, 8);
  CHECK(mode.mode() == BenchmarkPhase::Preparing && mode.run() == 2);
  ack.kind = BenchmarkAckKind::Quiet; ack.run = 1;
  mode.acknowledge(ack, 9);
  CHECK(mode.mode() == BenchmarkPhase::Preparing);
  ack.run = 2;
  mode.acknowledge(ack, 10);
  mode.captureDone();
  ack.kind = BenchmarkAckKind::OutputDone;
  mode.acknowledge(ack, 11);
  CHECK(mode.mode() == BenchmarkPhase::Finished);
  mode.requestAgain(false, 12);
  CHECK(mode.mode() == BenchmarkPhase::Finished);
  mode.requestAgain(true, 13);
  CHECK(mode.mode() == BenchmarkPhase::Preparing && mode.run() == 3);
  mode.poll(2000012);
  CHECK(mode.mode() == BenchmarkPhase::Preparing);
  mode.poll(2000013);
  CHECK(mode.mode() == BenchmarkPhase::Results && mode.preparationFailed());
  ack.kind = BenchmarkAckKind::Quiet; ack.run = 3;
  mode.acknowledge(ack, 2000014);
  CHECK(mode.mode() == BenchmarkPhase::Results);
  BenchmarkData data;
  data.begin();
  data.consume(reading(0)); // Discard.
  CHECK(data.summary().attempted == 0);
  for (unsigned i = 1; i <= 50; ++i) data.consume(reading(i));
  CHECK(data.done());
  auto summary = data.summary();
  CHECK(summary.attempted == 50 && summary.successful == 50);
  CHECK(summary.durations.count == 50);
  CHECK(summary.durations.minimum == 18000 && summary.durations.maximum == 18000);
  CHECK(summary.durations.mean() == 18000);
  CHECK(summary.intervals.count == 49);
  CHECK(summary.intervals.minimum == 19000 && summary.intervals.maximum == 19000);
  CHECK(summary.intervals.mean() == 19000);
  data.consume(reading(51)); // Results immutable after completion.
  CHECK(data.samples()[49].readEndUs == reading(50).readEndUs);
  data.begin();
  data.consume(reading(0));
  data.consume(reading(1));
  data.consume(reading(2, SampleStatus::Timeout));
  summary = data.summary();
  CHECK(data.done() && summary.attempted == 2 && summary.successful == 1);
  CHECK(summary.error == SampleStatus::Timeout);
  CHECK(summary.intervals.count == 0);
  for (unsigned i = 2; i < 50; ++i)
    CHECK(data.samples()[i].status == SampleStatus::NotRun);
  data.begin();
  data.consume(reading(0, SampleStatus::I2cError)); // Discard failure preserved.
  CHECK(data.done() && data.summary().successful == 0);
  CHECK(data.summary().attempted == 1);
  CHECK(data.summary().intervals.count == 0);
  data.failPreparation();
  CHECK(data.done() && data.summary().preparationFailed);
  CHECK(data.summary().attempted == 0);
  CHECK(data.summary().durations.count == 0);
  for (const auto& s : data.samples()) CHECK(s.status == SampleStatus::NotRun);
  // Regression: A arrives after the sensor API returns while in Finished.
  BenchmarkSession flow;
  BenchmarkData buffer;
  flow.start(0);
  BenchmarkAck quiet;
  quiet.kind = BenchmarkAckKind::Quiet; quiet.run = flow.run();
  flow.acknowledge(quiet, 1);
  buffer.begin();
  for (unsigned i = 0; i <= 50; ++i) {
    const bool notify = processBenchmarkSample(flow, buffer, reading(i), false);
    CHECK(notify == (i == 50));
  }
  CHECK(flow.mode() == BenchmarkPhase::Results);
  BenchmarkAck printed;
  printed.kind = BenchmarkAckKind::OutputDone; printed.run = flow.run();
  flow.acknowledge(printed, reading(50).readEndUs + 1);
  CHECK(flow.mode() == BenchmarkPhase::Finished);
  const auto restart = reading(100);
  CHECK(processBenchmarkSample(flow, buffer, restart, true)); // Must publish Preparing.
  CHECK(flow.mode() == BenchmarkPhase::Preparing && flow.run() == 2);
  CHECK(buffer.summary().successful == 50); // Old results stay fixed until quiet ACK.
  quiet.run = flow.run();
  flow.acknowledge(quiet, restart.readEndUs + 1);
  CHECK(flow.mode() == BenchmarkPhase::Capturing);
  buffer.begin();
  for (unsigned i = 101; i <= 151; ++i)
    processBenchmarkSample(flow, buffer, reading(i), false);
  CHECK(flow.mode() == BenchmarkPhase::Results);
  CHECK(!flow.preparationFailed());
  CHECK(buffer.summary().successful == 50);
  CHECK(buffer.samples()[49].status == SampleStatus::Ok);
  std::printf("PASS: benchmark session, %u checks\n", checks);
}
