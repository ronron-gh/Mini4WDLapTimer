# ラップタイマー中核テスト

Arduino・M5Stack・センサーを使用せず、合成距離と時刻で8シナリオを検証します。標準C++11とGCC互換のホストコンパイラ（g++またはclang++）が必要です。

プロジェクト直下から実行します。

```text
python test/lap_timer/run_tests.py
python test/lap_timer/run_tests.py --compiler C:/path/to/g++.exe
```

生成物はGit管理対象外の.pio/host-testsに保存します。失敗時は非ゼロで終了し、全検証を通過した場合だけPASSと検証数を出します。PlatformIOのESP32環境は変更しません。通常のpio runはファームウェアをビルドしますが、このホストテストを実行しません。

2026-10-10の作業環境ではホストC++コンパイラが見つからず、WSLもアクセス拒否となったため、テスト実行は未実施です。ESP32のコンパイラによるテストソースのコンパイル、および中核を含むファームウェアのビルド・リンクは確認済みです。これはテストの実行成功を意味しません。

## WSL2での実行結果（2026-10-10）

サンドボックス外からUbuntuへアクセスし、g++ 13.3.0で実行成功しました。8シナリオ、246チェックを通過しています。上記の未実施記録は追加検証で解消しています。

```text
wsl.exe -d Ubuntu --cd /mnt/c/Users/motoh/OneDrive/Documents/PlatformIO/Projects/Mini4WDLapTimer --exec python3 test/lap_timer/run_tests.py
```

## 第2段階の追加検証

同じrun_tests.pyでtest/measurement_flow/test_main.cppもビルド・実行します。取得エラー変換の優先順位、A操作の取得前・取得中・取得後の扱い、旧値を新校正へ混ぜないこと、異常優先と記録保持を確認します。2026-10-10のWSL2実行では、中核246チェックと取得連携34チェック、合計280チェックを通過しました。RTOSの実スケジューリングと実センサー動作の検証ではありません。
