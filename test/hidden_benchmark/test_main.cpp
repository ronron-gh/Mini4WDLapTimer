#include "HiddenBenchmark.h"
#include "HiddenAcquisition.h"
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
  HiddenHold hold;
  CHECK(!hold.update(true, true, 1, 0));
  CHECK(!hold.update(true, true, 1, 2999000));
  CHECK(hold.update(true, true, 1, 3000000));
  CHECK(!hold.update(true, true, 1, 4000000));
  CHECK(!hold.update(false, true, 1, 4000001));
  CHECK(!hold.update(true, false, 1, 5000000));
  CHECK(!hold.update(true, true, 1, 9000000)); // Started outside idle.
  hold.update(false, true, 1, 9000001);
  hold.update(true, true, 1, 10000000);
  CHECK(!hold.update(true, true, 2, 13000000)); // Generation change.
  CHECK(!hold.update(true, true, 1, 14000000));
  hold.update(false, true, 1, 15000000);
  hold.update(true, true, 1, 16000000);
  CHECK(!hold.update(true, false, 1, 17000000)); // A cancels eligibility.
  CHECK(!hold.update(true, true, 1, 20000000));
  HiddenBenchmark mode;
  mode.request(true, true, true, 0);
  CHECK(mode.mode() == HiddenMode::Normal); // A wins.
  mode.request(false, true, false, 1);
  CHECK(mode.mode() == HiddenMode::Normal);
  mode.request(false, true, true, 2);
  CHECK(mode.mode() == HiddenMode::Preparing);
  CHECK(mode.run() == 1);
  BenchmarkAck ack;
  ack.kind = BenchmarkAckKind::Quiet; ack.run = 0;
  mode.acknowledge(ack, 3);
  CHECK(mode.mode() == HiddenMode::Preparing);
  mode.request(true, false, false, 4);
  ack.run = 1;
  mode.acknowledge(ack, 5);
  CHECK(mode.mode() == HiddenMode::Capturing);
  mode.request(true, false, false, 6); // Multiple requests collapse.
  mode.captureDone();
  CHECK(mode.mode() == HiddenMode::Results);
  mode.request(true, false, false, 7);
  CHECK(mode.mode() == HiddenMode::Results); // Cannot overwrite output.
  ack.kind = BenchmarkAckKind::OutputDone;
  mode.acknowledge(ack, 8);
  CHECK(mode.mode() == HiddenMode::Preparing && mode.run() == 2);
  ack.kind = BenchmarkAckKind::Quiet; ack.run = 1;
  mode.acknowledge(ack, 9);
  CHECK(mode.mode() == HiddenMode::Preparing); // Stale ACK ignored.
  ack.run = 2;
  mode.acknowledge(ack, 10);
  mode.captureDone();
  ack.kind = BenchmarkAckKind::OutputDone;
  mode.acknowledge(ack, 11);
  CHECK(mode.mode() == HiddenMode::Finished);
  mode.request(false, true, false, 12);
  CHECK(mode.mode() == HiddenMode::Finished);
  mode.request(true, false, false, 13);
  CHECK(mode.mode() == HiddenMode::Preparing && mode.run() == 3);
  mode.poll(2000012);
  CHECK(mode.mode() == HiddenMode::Preparing);
  mode.poll(2000013);
  CHECK(mode.mode() == HiddenMode::Results && mode.preparationFailed());
  ack.kind = BenchmarkAckKind::Quiet; ack.run = 3;
  mode.acknowledge(ack, 2000014);
  CHECK(mode.mode() == HiddenMode::Results);

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
  HiddenBenchmark flow;
  BenchmarkData buffer;
  flow.request(false, true, true, 0);
  BenchmarkAck quiet;
  quiet.kind = BenchmarkAckKind::Quiet; quiet.run = flow.run();
  flow.acknowledge(quiet, 1);
  buffer.begin();
  for (unsigned i = 0; i <= 50; ++i) {
    const bool notify = processHiddenSample(flow, buffer, reading(i), false);
    CHECK(notify == (i == 50));
  }
  CHECK(flow.mode() == HiddenMode::Results);
  BenchmarkAck printed;
  printed.kind = BenchmarkAckKind::OutputDone; printed.run = flow.run();
  flow.acknowledge(printed, reading(50).readEndUs + 1);
  CHECK(flow.mode() == HiddenMode::Finished);
  const auto restart = reading(100);
  CHECK(processHiddenSample(flow, buffer, restart, true)); // Must publish Preparing.
  CHECK(flow.mode() == HiddenMode::Preparing && flow.run() == 2);
  CHECK(buffer.summary().successful == 50); // Old results stay fixed until quiet ACK.
  quiet.run = flow.run();
  flow.acknowledge(quiet, restart.readEndUs + 1);
  CHECK(flow.mode() == HiddenMode::Capturing);
  buffer.begin();
  for (unsigned i = 101; i <= 151; ++i)
    processHiddenSample(flow, buffer, reading(i), false);
  CHECK(flow.mode() == HiddenMode::Results);
  CHECK(!flow.preparationFailed());
  CHECK(buffer.summary().successful == 50);
  CHECK(buffer.samples()[49].status == SampleStatus::Ok);
  std::printf("PASS: hidden benchmark, %u checks\n", checks);
}
