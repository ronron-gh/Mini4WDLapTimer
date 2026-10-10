#include "HiddenBenchmark.h"
namespace lap_timer {
void HiddenBenchmark::begin(std::uint64_t nowUs) {
  mode_ = HiddenMode::Preparing;
  ++run_;
  preparingUs_ = nowUs;
  preparationFailed_ = false;
  rerun_ = false;
}
void HiddenBenchmark::request(bool a, bool c, bool canEnter, std::uint64_t nowUs) {
  if (mode_ == HiddenMode::Normal) {
    if (!a && c && canEnter) begin(nowUs);
  } else if (a) {
    if (mode_ == HiddenMode::Finished) begin(nowUs);
    else rerun_ = true;
  }
}
void HiddenBenchmark::poll(std::uint64_t nowUs) {
  if (mode_ == HiddenMode::Preparing && nowUs - preparingUs_ >= 2000000) {
    preparationFailed_ = true;
    mode_ = HiddenMode::Results;
  }
}
void HiddenBenchmark::acknowledge(const BenchmarkAck& ack, std::uint64_t nowUs) {
  poll(nowUs);
  if (ack.run != run_) return;
  if (ack.kind == BenchmarkAckKind::Quiet && mode_ == HiddenMode::Preparing) {
    mode_ = HiddenMode::Capturing;
  } else if (ack.kind == BenchmarkAckKind::OutputDone && mode_ == HiddenMode::Results) {
    if (rerun_) begin(nowUs);
    else mode_ = HiddenMode::Finished;
  }
}
void HiddenBenchmark::captureDone() {
  if (mode_ == HiddenMode::Capturing) mode_ = HiddenMode::Results;
}
bool HiddenHold::update(bool pressed, bool eligible, std::uint64_t generation,
                        std::uint64_t nowUs) {
  if (!pressed) { pressed_ = false; return false; }
  if (!pressed_) {
    pressed_ = true;
    eligible_ = eligible;
    fired_ = false;
    startedUs_ = nowUs;
    generation_ = generation;
  }
  if (!eligible || generation != generation_) eligible_ = false;
  if (eligible_ && !fired_ && nowUs - startedUs_ >= 3000000) {
    fired_ = true;
    return true;
  }
  return false;
}
}  // namespace lap_timer
