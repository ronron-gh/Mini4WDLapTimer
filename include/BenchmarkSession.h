#pragma once
#include <cstdint>
namespace lap_timer {
enum class BenchmarkPhase : std::uint8_t { Preparing, Capturing, Results, Finished };
enum class BenchmarkAckKind : std::uint8_t { Quiet, OutputDone };
struct BenchmarkAck {
  BenchmarkAckKind kind = BenchmarkAckKind::Quiet;
  std::uint64_t run = 0;
};
class BenchmarkSession {
 public:
  void start(std::uint64_t nowUs);
  void requestAgain(bool a, std::uint64_t nowUs);
  void acknowledge(const BenchmarkAck& ack, std::uint64_t nowUs);
  void poll(std::uint64_t nowUs);
  void captureDone();
  BenchmarkPhase mode() const { return mode_; }
  std::uint64_t run() const { return run_; }
  bool preparationFailed() const { return preparationFailed_; }
 private:
  void begin(std::uint64_t nowUs);
  BenchmarkPhase mode_ = BenchmarkPhase::Finished;
  std::uint64_t run_ = 0;
  std::uint64_t preparingUs_ = 0;
  bool rerun_ = false;
  bool preparationFailed_ = false;
};
}  // namespace lap_timer
