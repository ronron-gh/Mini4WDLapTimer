#include "BenchmarkSession.h"
namespace lap_timer {
void BenchmarkSession::begin(std::uint64_t nowUs) {
  mode_ = BenchmarkPhase::Preparing;
  ++run_;
  preparingUs_ = nowUs;
  preparationFailed_ = false;
  rerun_ = false;
}
void BenchmarkSession::start(std::uint64_t nowUs) { begin(nowUs); }
void BenchmarkSession::requestAgain(bool a, std::uint64_t nowUs) {
  if (!a) return;
  if (mode_ == BenchmarkPhase::Finished) begin(nowUs);
  else rerun_ = true;
}
void BenchmarkSession::poll(std::uint64_t nowUs) {
  if (mode_ == BenchmarkPhase::Preparing && nowUs - preparingUs_ >= 2000000) {
    preparationFailed_ = true;
    mode_ = BenchmarkPhase::Results;
  }
}
void BenchmarkSession::acknowledge(const BenchmarkAck& ack, std::uint64_t nowUs) {
  poll(nowUs);
  if (ack.run != run_) return;
  if (ack.kind == BenchmarkAckKind::Quiet && mode_ == BenchmarkPhase::Preparing) {
    mode_ = BenchmarkPhase::Capturing;
  } else if (ack.kind == BenchmarkAckKind::OutputDone && mode_ == BenchmarkPhase::Results) {
    if (rerun_) begin(nowUs);
    else mode_ = BenchmarkPhase::Finished;
  }
}
void BenchmarkSession::captureDone() {
  if (mode_ == BenchmarkPhase::Capturing) mode_ = BenchmarkPhase::Results;
}
}  // namespace lap_timer
