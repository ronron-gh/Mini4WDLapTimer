#pragma once
#include <cstdint>
namespace lap_timer {
enum class HiddenMode : std::uint8_t { Normal, Preparing, Capturing, Results, Finished };
enum class BenchmarkAckKind : std::uint8_t { Quiet, OutputDone };
struct BenchmarkAck {
  BenchmarkAckKind kind = BenchmarkAckKind::Quiet;
  std::uint64_t run = 0;
};
class HiddenBenchmark {
 public:
  void request(bool a, bool c, bool canEnter, std::uint64_t nowUs);
  void acknowledge(const BenchmarkAck& ack, std::uint64_t nowUs);
  void poll(std::uint64_t nowUs);
  void captureDone();
  HiddenMode mode() const { return mode_; }
  std::uint64_t run() const { return run_; }
  bool preparationFailed() const { return preparationFailed_; }
 private:
  void begin(std::uint64_t nowUs);
  HiddenMode mode_ = HiddenMode::Normal;
  std::uint64_t run_ = 0;
  std::uint64_t preparingUs_ = 0;
  bool rerun_ = false;
  bool preparationFailed_ = false;
};
class HiddenHold {
 public:
  bool update(bool pressed, bool eligible, std::uint64_t generation,
              std::uint64_t nowUs);
 private:
  bool pressed_ = false;
  bool eligible_ = false;
  bool fired_ = false;
  std::uint64_t startedUs_ = 0;
  std::uint64_t generation_ = 0;
};
}  // namespace lap_timer
