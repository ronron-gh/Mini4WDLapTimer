#include "TimerDisplay.h"
#include <M5Unified.h>
#include <cstdio>

namespace lap_timer {
namespace {
constexpr std::uint32_t kBackground = 0x0b1420;
constexpr std::uint32_t kText = 0xedf4ff;
constexpr std::uint32_t kMuted = 0xa8b6c9;
std::uint32_t stateColor(State state) {
  if (state == State::Error) return 0xff7d85;
  if (state == State::Running) return 0x62e5ac;
  if (state == State::Complete) return 0x68b8ff;
  return 0xf6c65b;
}
void font(bool japanese) {
  M5.Display.setTextSize(1);
  if (japanese) M5.Display.setFont(&fonts::lgfxJapanGothic_12);
  else M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextDatum(lgfx::textdatum_t::top_left);
}
void numericText(const char* text, int right, int y, int width, float preferredSize) {
  font(false);
  float scale = preferredSize;
  M5.Display.setTextSize(scale);
  while (scale > 1 && M5.Display.textWidth(text) > width) {
    scale -= 0.25f;
    M5.Display.setTextSize(scale);
  }
  if (M5.Display.textWidth(text) > width) text = "OVF";
  M5.Display.drawRightString(text, right, y);
  M5.Display.setTextSize(1);
}
void timeText(std::uint64_t cs, char* buffer, std::size_t size) {
  formatCentiseconds(cs, buffer, size);
}
bool highlighted(const DisplayModel& model, std::size_t index) {
  return model.lapCount > 0 && index + 1 == model.lapCount &&
         model.rows[index].confirmed;
}
}  // namespace

void TimerDisplay::renderLcd(const DisplayModel& model) {
  const bool all = !drawn_ || previous_.generation != model.generation;
  auto& lcd = M5.Display;
  if (all) {
    lcd.setRotation(1);
    lcd.setTextWrap(false, false);
    lcd.fillScreen(kBackground);
    lcd.fillRect(0, 48, 320, 16, 0x243246);
    font(true);
    lcd.setTextColor(kText);
    lcd.drawString("周", 16, 49);
    lcd.drawRightString("ラップ(s)", 168, 49);
    lcd.drawRightString("累積(s)", 306, 49);
  }
  if (all || previous_.state != model.state || previous_.lapCount != model.lapCount) {
    lcd.fillRect(0, 0, 320, 20, 0x1b293b);
    font(true);
    lcd.setTextColor(stateColor(model.state));
    lcd.drawString(stateLabel(model.state), 10, 3);
    char count[12];
    std::snprintf(count, sizeof(count), "%u/10", static_cast<unsigned>(model.lapCount));
    font(false);
    lcd.setTextColor(kText);
    lcd.drawRightString(count, 306, 6);
  }
  if (all || previous_.elapsedCs != model.elapsedCs) {
    lcd.fillRect(0, 20, 320, 28, kBackground);
    font(true);
    lcd.setTextColor(kMuted);
    lcd.drawString("経過", 10, 28);
    char text[32];
    char value[28];
    timeText(model.elapsedCs, value, sizeof(value));
    std::snprintf(text, sizeof(text), "%s s", value);
    lcd.setTextColor(kText);
    numericText(text, 306, 24, 246, 2.5f);
  }
  for (std::size_t i = 0; i < kLapCount; ++i) {
    if (!all && sameRow(previous_.rows[i], model.rows[i]) &&
        highlighted(previous_, i) == highlighted(model, i)) continue;
    const auto& row = model.rows[i];
    const int y = 64 + static_cast<int>(i) * 15;
    lcd.fillRect(0, y, 320, 15,
        highlighted(model, i) ? 0x14382f : (i % 2 ? 0x101b29 : kBackground));
    font(false);
    lcd.setTextColor(kMuted);
    char number[8];
    std::snprintf(number, sizeof(number), "%u", static_cast<unsigned>(i + 1));
    lcd.drawRightString(number, 28, y + 3);
    char lap[28] = "--.--";
    char total[28] = "--.--";
    if (row.visible) {
      timeText(row.lapCs, lap, sizeof(lap));
      timeText(row.elapsedCs, total, sizeof(total));
    }
    lcd.setTextColor(row.confirmed ? kText : kMuted);
    numericText(lap, 168, y + 3, 128, 1.5f);
    numericText(total, 306, y + 3, 128, 1.5f);
  }
  if (all || previous_.state != model.state || previous_.error != model.error ||
      previous_.calibrationCount != model.calibrationCount) {
    lcd.fillRect(0, 216, 320, 24, kBackground);
    lcd.fillRect(0, 216, 108, 24, 0x273e59);
    font(true);
    lcd.setTextColor(kText);
    char action[32];
    std::snprintf(action, sizeof(action), "A %s", actionLabel(model.state));
    lcd.drawString(action, 8, 222);
    if (model.error != Error::None) {
      font(false);
      lcd.setTextColor(stateColor(State::Error));
      lcd.drawString(errorName(model.error), 114, 224);
    } else if (model.state == State::Calibrating) {
      char progress[40];
      std::snprintf(progress, sizeof(progress), "基準取得 %u/10",
                    static_cast<unsigned>(model.calibrationCount));
      lcd.drawString(progress, 114, 222);
    } else if (model.state == State::Armed) {
      lcd.drawString("車体の通過を待っています", 114, 222);
    } else if (model.state == State::Complete) {
      lcd.drawString("10周の結果を保持", 114, 222);
    }
  }
  previous_ = model;
  drawn_ = true;
}

void TimerDisplay::renderBenchmarkPreparing() {
  auto& lcd = M5.Display;
  lcd.setRotation(1);
  lcd.fillScreen(kBackground);
  font(true);
  lcd.setTextColor(kText);
  lcd.drawString("周期測定中", 12, 8);
  lcd.drawString("50回分をRAMへ記録します", 12, 52);
  lcd.drawString("測定中は画面更新を停止", 12, 80);
  lcd.drawString("A: 終了後の再測定を予約", 12, 190);
  lcd.drawString("本体リセットで通常画面へ", 12, 220);
}
void TimerDisplay::renderBenchmarkResult(const BenchmarkSummary& summary) {
  auto& lcd = M5.Display;
  lcd.fillScreen(kBackground);
  font(true);
  lcd.setTextColor(0x68b8ff);
  lcd.drawString("周期測定終了", 12, 8);
  lcd.setTextColor(kText);
  char text[64];
  std::snprintf(text, sizeof(text), "取得 %u/50  成功 %u",
      static_cast<unsigned>(summary.attempted), static_cast<unsigned>(summary.successful));
  lcd.drawString(text, 12, 42);
  lcd.setTextColor(kMuted);
  lcd.drawString("取得間隔（単位 ms）", 12, 72);
  const char* labels[] = {"MIN", "MEAN", "MAX"};
  for (int i = 0; i < 3; ++i) {
    font(false);
    lcd.setTextColor(kText);
    lcd.drawString(labels[i], 12, 102 + i * 28);
    if (summary.intervals.count == 0) std::snprintf(text, sizeof(text), "--.--");
    else {
      const double value = i == 0 ? summary.intervals.minimum :
          i == 1 ? summary.intervals.mean() : summary.intervals.maximum;
      std::snprintf(text, sizeof(text), "%.2f", value / 1000.0);
    }
    numericText(text, 306, 98 + i * 28, 220, 2);
  }
  font(false);
  lcd.setTextColor(summary.preparationFailed || summary.error != SampleStatus::Ok ?
                   0xff7d85 : kMuted);
  lcd.drawString(summary.preparationFailed ? "BENCH_PREP_TIMEOUT" :
      summary.error == SampleStatus::Ok ? "CSV: Serial" : sampleStatusName(summary.error), 12, 192);
  lcd.fillRect(0, 216, 108, 24, 0x273e59);
  font(true);
  lcd.setTextColor(kText);
  lcd.drawString("A 再測定", 8, 222);
  lcd.drawString("本体リセットで復帰", 118, 222);
}

void TimerDisplay::outputBaseline(Stream& output, const DisplayModel& model) const {
  char distance[20];
  if (!formatBaselineDistance(model, distance, sizeof(distance))) return;
  output.printf("BASELINE SESSION=%llu SAMPLES=%u DISTANCE_MM=%s\n",
      static_cast<unsigned long long>(model.generation),
      static_cast<unsigned>(kBaselineSamples), distance);
}

void TimerDisplay::outputSerial(Stream& output, const DisplayModel& model) const {
  char elapsed[28];
  timeText(model.elapsedCs, elapsed, sizeof(elapsed));
  output.printf("STATE=%s SESSION=%llu LAP=%u/10 ELAPSED=%s s\n",
      stateName(model.state), static_cast<unsigned long long>(model.generation),
      static_cast<unsigned>(model.lapCount), elapsed);
  output.println("LAP,LAP_S,ELAPSED_S");
  for (std::size_t i = 0; i < kLapCount; ++i) {
    char lap[28] = "--.--";
    char total[28] = "--.--";
    if (model.rows[i].visible) {
      timeText(model.rows[i].lapCs, lap, sizeof(lap));
      timeText(model.rows[i].elapsedCs, total, sizeof(total));
    }
    output.printf("%u,%s,%s\n", static_cast<unsigned>(i + 1), lap, total);
  }
  output.printf("A=%s ERROR=%s", actionName(model.state), errorName(model.error));
  if (model.state == State::Calibrating)
    output.printf(" CAL=%u/10", static_cast<unsigned>(model.calibrationCount));
  output.println();
}
}  // namespace lap_timer
