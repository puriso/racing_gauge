# テスト / Tests

`pio test -e native` で、実機を使わずに以下の回帰テストを実行できます。GCC/G++ を PATH に追加してください。

- `test_racing_mode`: レーシングモードの開始条件・強制停止・輝度復帰
- `test_display`: 温度の異常値・復帰・最高値記録・メニュー中の記録継続・油温バーの描画範囲

`native_stubs` は描画呼び出しを記録するテスト用の代替実装です。実際の画面表示、I²C 通信、センサー精度は検証しません。
`test_ci_dummy` と `test_sensor_conversion` は Arduino 向けのため、native 環境の対象外です。

Run `pio test -e native` with GCC/G++ on PATH to test racing mode behavior and temperature display regressions without hardware.
The native stubs record drawing calls; they do not validate physical rendering, I²C communication, or sensor accuracy.
The Arduino suites (`test_ci_dummy` and `test_sensor_conversion`) are excluded from the native environment.
