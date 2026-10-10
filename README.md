# Mini4WDLapTimer

M5Stack Core ESP32とVL53L0X ToFセンサーを使うミニ四駆ラップタイマーです。通常用と開発用ベンチマークを、PlatformIOのビルド環境で切り替えます。

## 通常ラップタイマー

既定の環境はm5stack-core-esp32です。

```text
pio run
pio run -t upload
pio device monitor
```

Aボタンで未取得結果1件を読み捨て、コースまでの距離10件の平均を取得します。平均取得時はセンサー下から車体を離してください。基準から20mm以上短くなると通過を検出し、最初の通過で開始、以後10周まで記録します。離脱は短縮量10mm以下、検出後1秒間は再検出しません。

1画面に10周のラップ・累積時間と現在経過時間を表示します。Aでリセットと基準距離再取得。基準距離の確定平均はSerialへ1回出力します。デバッグ用のラップ表出力はsrc/LapTimerTasks.cppの「Debug lap output」2ブロックのコメントを外すと再有効化できます。Cボタンは使用しません。

## ベンチマーク

```text
pio run -e m5stack-core-esp32-benchmark
pio run -e m5stack-core-esp32-benchmark -t upload
pio device monitor -e m5stack-core-esp32-benchmark
```

起動直後に自動で50回測定し、時刻・距離をRAMに記録してからCSVと取得周期の集計をSerialへ出します。LCDには取得数・成功数・間隔の最小／平均／最大を表示します。測定中はLCD・Serial出力を止めます。

Aで再測定できます。準備・測定・結果出力中のAは、出力終了後の次の1回を予約します。Cは使用しません。本体リセットでも自動測定を再実行します。通常ラップタイマーへ戻すには、通常環境のファームウェアを書き込んでください。

## 構成・設定

LAP_TIMER_BENCHMARK_MODEは通常環境で0、ベンチマーク環境で1。ソースフィルタで各タスク実装を選びます。マクロだけを別環境と食い違う値へ変更すると、タスク実装の静的検証でビルドエラーになります。

- TimerTasks.cpp：共通起動・開始ゲート・A入力
- LapTimerTasks.cpp：通常の測定・表示
- BenchmarkTasks.cpp：専用の測定・出力
- SensorReader：センサー初期化とContinuous開始を起動時に一度だけ実行
- 測定Core 1・優先度3、入力Core 0・優先度2、表示Core 0・優先度1

測定時間設定20ms、I²C 400kHz、通信タイムアウト20ms、取得タイムアウト1000ms。時刻は起動後のµs、ラップ表示は秒の小数2桁。再測定・A操作ではセンサーを停止・再初期化しません。

## 仕様・検証

- [要求仕様](docs/requirement.md)
- [機能設計](docs/functional_design.md)
- [コンパイル時切り替えステアリング](.steering/20261010-build-mode.md)
- [ホストテスト](test/lap_timer/README.md)

両環境のビルドと567チェックを確認済みです。切り替え変更後の実機動作はユーザー確認済みです。測定周期・スタック使用量などの定量評価は未実施です。
