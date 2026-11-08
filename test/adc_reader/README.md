ADC通信異常の回帰テストは、Python 3とC++17対応コンパイラで実行します。

```sh
pio pkg install -e m5stack-cores3
python3 test/adc_reader/run_tests.py
```

本番のADC読み取り・センサー取得処理を使い、I2C装置・時刻・IMUをスタブに置き換えます。
Adafruitの定数は、PlatformIOが取得した実際のヘッダーを使います。
未接続、タイムアウト、通信途中の失敗、再接続、時刻の折り返し、異常サンプルの破棄を確認します。

修正前の無期限待機を再現する場合は、次のコマンドを実行します。

```sh
python3 test/adc_reader/run_tests.py --check-legacy
```

取得済みのAdafruitドライバーを未接続の装置で実行し、0.5秒でプロセスを停止します。
実機のI2C信号やバス自体のタイムアウトは、このテストの対象外です。
