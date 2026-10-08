# Mini4WDLapTimer

M5Stack Core ESP32とVL53L0X ToFセンサを使うラップタイマー開発用プロジェクトです。現在はPololu VL53L0Xライブラリで測定周期を調べるベンチマークを実装しています。

## 使い方

1. PlatformIOでビルド・書き込みを行います（`pio run`、`pio run -t upload`）。
2. 115200bpsでシリアルモニタを開きます（`pio device monitor`）。
3. 起動時に50回連続測定します。再測定はAボタンを押します。

`setup()`から`initializeSensor()`を一度だけ呼び、初期化・校正、測定時間設定、`startContinuous()`によるback-to-back連続測定の開始を行います。Aボタンでは初期化・再設定・再起動を行いません。`readRangeContinuousMillimeters()`で50回分をRAMの固定配列に記録してCSVと集計を出します。`stopContinuous()`は呼ばず、ログ出力中やAボタンの待機中も連続測定を維持します。各バッチの記録開始前に未取得の結果を1回読み捨て、待機中の結果が50回分の記録に混ざることを避けます。読み捨ても失敗した場合はバッチを中止します。測定中はLCD表示・シリアル出力・固定待ち時間を入れません。完了待ちのポーリングはライブラリに任せます。アプリ側のレジスタ直接操作はありません。

## 設定

`src/main.cpp`の定数で変更できます。

- `kTimingBudgetUs = 20000`：測定時間の設定値（µs）。Single exampleのHIGH_SPEED設定に合わせた20msです。`0`にするとライブラリの初期設定のままで比較できます。設定値と実際の周期は同一とは限りません。
- `kI2cFrequencyHz = 400000`：I²Cクロック（Hz）。元の測定との比較でContinuousモードだけの効果を切り分ける場合は、元のクロックにも合わせてください。
- `kMeasurementTimeoutMs = 1000`：ライブラリの測定待ちタイムアウト（ms）。
- `kI2cTimeoutMs = 20`：Wireの通信タイムアウト（ms）。

設定値がライブラリに拒否された場合は`CONFIG_ERROR`としてバッチを中止します。8ms動作を保証する設定ではありません。測定時間を短くする場合は距離の安定性も実機で確認してください。ライブラリ1.3.1では20msの固定下限は撤廃されていますが、現在の測定シーケンスに必要な時間より短い設定は拒否されます。

## ログ

`index,read_start_us,read_end_us,read_duration_us,interval_us,distance_mm,status`

- `read_start_us`：距離取得APIを呼び出す直前の時刻。センサの測定開始時刻ではありません。
- `read_end_us`：距離取得APIから戻った直後の時刻。完了待ち、距離読み取り、完了フラグのクリアを含みます。
- `read_duration_us`：API呼び出しの所要時間。センサ自体の測定時間とは異なります。
- `interval_us`：前の行とのAPI終了時刻の差。実効取得周期を見る項目です。先頭行は空欄で、周期の集計から除外します。
- `distance_mm`：距離（mm）。
- `status`：`OK`、`TIMEOUT`、`I2C_ERROR`、`READ_ERROR`（タイムアウト以外で65535を返した）、`INIT_ERROR`、`CONFIG_ERROR`、`NOT_RUN`。

前バージョンの`start_us,ready_us,end_us,duration_us,range_status`は廃止しました。高水準APIでは完了検出直後の時刻と距離品質ステータスを取得できないためです。

エラーを検出した場合は残りの取得を中止し、未実行行を`NOT_RUN`にします。50行は常に出力します。無効な距離・所要時間・間隔は空欄です。集計は取得成功行のAPI所要時間と隣り合う取得成功行の間隔について最小・平均・最大値（µs）を出します。起動処理や読み捨ての異常はCSVの前にコメントで出します。起動処理に失敗した場合はAボタンで再初期化せず、原因を解消して本体をリセットしてください。

時刻にはESP32の`esp_timer_get_time()`を使用します。起動後の経過時間を64bitのµs単位で記録し、カレンダーの実時刻は取得しません。

`OK`は検出できる取得エラーがなかったことを意味します。距離品質の判定はありません。ライブラリの`last_status`は最後のI²C送信結果のみで、途中の通信失敗や読み取りバイト数不足をすべて検出するものではありません。

参考：[Pololu VL53L0Xライブラリ](https://github.com/pololu/vl53l0x-arduino)、インストール済みライブラリのContinuous／Single example。
