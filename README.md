# M5Stack CoreS3 Racing Gauge

[![PlatformIO Build](https://github.com/puriso/racing_gauge/actions/workflows/pio-build.yml/badge.svg?branch=main)](https://github.com/puriso/racing_gauge/actions/workflows/pio-build.yml)

**油圧・水温・油温を、ひとつの画面で。** M5Stack CoreS3とADS1015を使用した、車載用デジタルメーターです。
アナログ風ゲージ、低油圧警告、最高値の確認を備え、走行中の状態把握を補助します。

A digital oil-pressure and temperature dashboard for M5Stack CoreS3 + ADS1015. [English quick start](#english-quick-start)

[等速の動作確認ページを開く](https://puriso.github.io/racing_gauge/)

油圧3〜6barの連続変動、2.0Gでの油圧急落、Rモードの保持・解除、温度の上昇・下降を、
PRごとの等速動画で確認できます。[シミュレーターと録画の使い方](simulator/README.md)。
生ADCカウント・IMU各軸・照度を本番のセンサー処理へ渡し、変換・補正・取得周期・平均化から表示まで確認します。
シミュレーションと動画は24FPS・1.0倍速です。

[Real-time visual review](https://puriso.github.io/racing_gauge/): continuous pressure changes, circuit pressure drops,
racing-mode transitions, and heating/cooling scenarios. All videos play at normal speed.

<img src="https://github.com/user-attachments/assets/d3a8bfdc-0bba-4519-b64a-f31a1ec9a9f4" alt="Racing Gaugeの表示例" width="640">

[機能](#機能) · [セットアップ](#セットアップ) · [配線](#ハードウェアと配線) · [設定](#設定) · [開発・検証](#開発と検証)

## 機能

| 表示 | 内容 | 初期設定の目盛・しきい値 |
| --- | --- | --- |
| 油圧 | アナログ風ゲージと数値 | 0–10 bar、8 bar以上で赤色 |
| 水温 | アナログ風ゲージと数値 | 85–115°C、110°C以上で赤色 |
| 油温 | 画面上部のバーと数値 | 80–130°C、120°C以上で赤色 |
| 詳細画面 | 最高油圧・最高水温・最高油温、直近の低油圧イベント、照度 | 画面タップで開閉 |

- **低油圧警告**：水平Gが1.0 Gを超え、表示用の油圧が3.0 bar以下の状態が500ms継続すると、`LOW`を表示します。警告条件の最終検知から3秒間の表示保持を設けています。
- **レーシングモード**：水平Gが1.0 G以上の状態が100ms継続すると、3分間、輝度を最大にして`R`を表示します。
- **自動調光**：周囲光に応じて3段階の輝度に切り替えます。初期設定では有効です。
- **デモモード**：外付けADC・センサーを接続せずに表示を確認できます。初期設定では無効です。

油圧の画面上の単位`x100kPa`はbarと同じ大きさです（1 bar = 100 kPa）。
最高値・イベント履歴はメモリ上に保持し、再起動するとリセットされます。

## セットアップ

### 1. 開発環境を用意する

[PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)をインストールします。
VS CodeのPlatformIO IDEを利用する場合は、内蔵のPlatformIOターミナルで以下を実行できます。

```sh
git clone https://github.com/puriso/racing_gauge.git
cd racing_gauge
pio run -e m5stack-cores3
```

生成されるファームウェアは`.pio/build/m5stack-cores3/firmware.bin`です。
ボード定義とライブラリの指定は[platformio.ini](platformio.ini)で管理しています。
`M5Unified@^0.2.7`などはバージョン範囲の指定であり、特定のバージョンへの固定ではありません。

### 2. USB接続して書き込む

```sh
pio device list
```

一覧からCoreS3のポートを確認し、書き込み時に指定します。以下はmacOSの例です。
`/dev/cu.usbmodemXXXX`を実際のポート名に置き換えてください。

```sh
pio run -e m5stack-cores3 -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor --port /dev/cu.usbmodemXXXX --baud 115200
```

Windowsでは`COM11`、Linuxでは`/dev/ttyACM0`など、環境に合ったポート名を使用します。
現在の`platformio.ini`には`upload_port = COM11`が設定されているため、異なる環境では上記の`--upload-port`で上書きしてください。
コマンドの詳細は[PlatformIOの公式リファレンス](https://docs.platformio.org/en/stable/core/userguide/cmd_run.html)を参照してください。

### 3. 表示とセンサー値を確認する

起動直後はIMUの安定化待ちとオフセット推定を行うため、本体を静止させてください。
センサーを接続する前に画面を確認したい場合は、[include/config.h](include/config.h)の`DEMO_MODE_ENABLED`を`1`に変更し、再ビルド・書き込みします。
実測に戻す際は`0`に戻してください。

## ハードウェアと配線

### 構成とチャンネル

| 機器・信号 | 使用対象・方式 | コード上の接続先 |
| --- | --- | --- |
| 本体 | M5Stack CoreS3 / ESP32-S3 | 320 × 240画素の内蔵ディスプレイ |
| ADC | ADS1015 / 12bit・4チャンネル | I²Cアドレス`0x48` |
| 油温 | Defi PDF00703Sを想定したNTC抵抗式 | ADS1015 A0 / CH0 |
| 水温 | Defi PDF00703Sを想定したNTC抵抗式 | ADS1015 A1 / CH1 |
| 油圧 | Defi PDF00903Sを想定した電圧出力式 | ADS1015 A2 / CH2 |
| 外部I²C SDA | CoreS3 GPIO9 | ADS1015 SDAとのレベル整合が必要 |
| 外部I²C SCL | CoreS3 GPIO8 | ADS1015 SCLとのレベル整合が必要 |

上記は[config.h](include/config.h)と[setup()](src/main.cpp)の割り当てです。
本体のコネクター・ピン配置は[M5Stack公式資料](https://docs.m5stack.com/en/core/CoreS3)と照合してください。

### 接続の考え方

温度入力は、5V側の10kΩ固定抵抗とGND側のNTCによる分圧を想定しています。
油圧入力は0.5–4.5Vの電圧を換算します。温度センサーを同じ電圧出力式として接続しないでください。

```mermaid
flowchart LR
    CORE["CoreS3<br/>GPIO9: SDA / GPIO8: SCL"]
    LEVEL["I²Cレベル変換<br/>3.3V ↔ 5V"]
    ADC["ADS1015<br/>VDD: 5V / ADDR: GND / 0x48"]
    SUPPLY["5V電源"]
    GND["共通GND"]
    RP_OIL["10kΩ固定抵抗"]
    RP_WATER["10kΩ固定抵抗"]
    OIL_NODE(("油温分圧点"))
    WATER_NODE(("水温分圧点"))
    OIL_NTC["油温NTC"]
    WATER_NTC["水温NTC"]
    PRESSURE["油圧センサー<br/>0.5–4.5V出力"]

    CORE <-->|"SDA / SCL"| LEVEL
    LEVEL <-->|"SDA / SCL"| ADC
    SUPPLY --> ADC
    SUPPLY --> RP_OIL --> OIL_NODE --> OIL_NTC --> GND
    SUPPLY --> RP_WATER --> WATER_NODE --> WATER_NTC --> GND
    SUPPLY --> PRESSURE
    PRESSURE --> GND
    OIL_NODE -->|"A0 / CH0"| ADC
    WATER_NODE -->|"A1 / CH1"| ADC
    PRESSURE -->|"A2 / CH2"| ADC
    CORE --- GND
    ADC --- GND
```

これは接続関係のブロック図です。レベル変換回路の電源・プルアップ抵抗は、使用する基板の回路図に従って接続してください。
レベル変換を内蔵するADC基板では、その基板の入出力仕様を確認します。

ESP32-S3の3.3V系GPIOと、5V駆動のADS1015のI²C信号にはレベル整合が必要です。
ADS1015のゲイン設定`±6.144V`も、電源電圧を超えるアナログ入力を許可するものではありません。
電気的条件は[ESP32-S3データシート](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf)と[ADS1015データシート](https://www.ti.com/lit/ds/symlink/ads1015.pdf)を参照してください。

### 電源設定

想定する実機の給電経路は、`車載12V → DinBaseのDC入力 → 内蔵降圧回路 → 5V → CoreS3・ADS1015`です。
DinBaseはDC 9–24V入力と内蔵の降圧回路を備えています。[DinBase公式資料](https://docs.m5stack.com/en/base/Base_DIN)
CoreS3〜ADS1015のI²C配線は、実機では3cm未満です。

**DC入力給電では、CoreS3から外部へ5Vを出す設定と給電方向を合わせる必要があります。**
[公式の電源サンプル](https://github.com/m5stack/M5CoreS3/blob/main/examples/Basic/power/power.ino)は、DC入力・Groveからの給電時に`cfg.output_power = false`を指定しています。
外部出力を強制ONにする処理の修正は[#382](https://github.com/puriso/racing_gauge/pull/382)で管理しています。
同PRでは`EXTERNAL_5V_OUTPUT_ENABLED`を共通設定にし、初期値を`false`にしています。取り込み状況を確認してください。

```cpp
// 修正前の外部出力指定
M5.Power.setExtOutput(true);
```

```cpp
// #382適用後のconfig.h：DinBaseのDC給電向けの既定値
constexpr bool EXTERNAL_5V_OUTPUT_ENABLED = false;

// setupの設定の要点
auto cfg = M5.config();
cfg.output_power = EXTERNAL_5V_OUTPUT_ENABLED;
CoreS3.begin(cfg);

// 後段の外部出力指定も、初期化設定と揃える
M5.Power.setExtOutput(EXTERNAL_5V_OUTPUT_ENABLED);
```

変更後はADC・センサー側の5V供給も実機で確認してください。
USB・バッテリーからCoreS3経由でセンサーへ5Vを出す構成では、同定数を`true`に変更して再ビルドします。
5Vピンへ直接外部給電する場合も、外部出力を有効にした状態で別電源を同時接続しないでください。

コードの`setBatteryCharge(false)`はPMU側への充電停止設定です。
DinBaseは別の充電ICも搭載するため、底座を含むすべての充電回路がこの指定で停止するとは扱いません。

## 設定

主な設定は[include/config.h](include/config.h)を編集して変更します。変更後は再ビルド・書き込みが必要です。

| 設定 | 初期値 | 内容 |
| --- | --- | --- |
| `DEMO_MODE_ENABLED` | `0` | デモ表示 |
| `SENSOR_OIL_PRESSURE_PRESENT` | `1` | 油圧センサーの使用 |
| `SENSOR_WATER_TEMP_PRESENT` | `1` | 水温センサーの使用 |
| `SENSOR_OIL_TEMP_PRESENT` | `1` | 油温センサーの使用 |
| `SENSOR_AMBIENT_LIGHT_PRESENT` | `1` | 周囲光センサーによる自動調光 |
| `DEBUG_MODE_ENABLED` | `0` | シリアルへのセンサー値・FPS出力 |
| `FPS_DISPLAY_ENABLED` | `0` | 画面上のFPS表示 |
| `VOLTAGE_DROP` | `0.137V` | 油圧・温度の換算に使う電圧降下補正 |
| `PRESSURE_SAMPLE_SIZE` | `5` | 油圧の平均サンプル数 |
| `WATER_TEMP_SAMPLE_SIZE` / `OIL_TEMP_SAMPLE_SIZE` | `2` | 温度の平均サンプル数 |

油圧は各フレームで取得し、直近5サンプルの平均に平滑化を適用します。
水温・油温はそれぞれ500msごとに取得し、直近2サンプル、約1秒分を平均してから表示を平滑化します。
温度平均は新しいサンプルが入る500msごとに更新されます。

低油圧警告・バーの赤色しきい値は[src/modules/low_warning.cpp](src/modules/low_warning.cpp)と[src/modules/display.cpp](src/modules/display.cpp)、
温度の取得間隔・換算係数は[src/modules/sensor.cpp](src/modules/sensor.cpp)で定義しています。

## センサーの換算と校正

### 油圧

補正後の電圧`V`に対して、`P [bar] = 2.5 × (V − 0.5)`で換算します。0.5V以下は0 barとして扱います。

| 補正後の電圧 | 換算油圧 |
| --- | --- |
| 0.50V | 0 bar |
| 1.30V | 2 bar |
| 2.50V | 5 bar |
| 4.50V | 10 bar |

ADC入力で4.9V以上の過電圧を検出した場合などは、油圧を0扱いにします。
この0表示だけでは、実際の低油圧と配線・センサーの異常を区別できません。

### 水温・油温

10kΩの基準抵抗、`R25 = 10kΩ`、`B = 3380K`のβ式を使用します。
以下は実装の換算モデルから求めた参考値であり、センサー製造元の保証値や実測の校正データではありません。

<details>
<summary>温度とNTC抵抗値の参考表</summary>

| 温度 | 抵抗値 |
| --- | --- |
| 0°C | 28,221.67Ω |
| 10°C | 18,230.65Ω |
| 20°C | 12,133.01Ω |
| 30°C | 8,294.71Ω |
| 40°C | 5,810.08Ω |
| 50°C | 4,160.37Ω |
| 60°C | 3,039.42Ω |
| 70°C | 2,261.49Ω |
| 80°C | 1,711.08Ω |
| 90°C | 1,314.67Ω |
| 100°C | 1,024.46Ω |
| 110°C | 808.78Ω |
| 120°C | 646.23Ω |

</details>

入力電圧が換算可能な範囲を外れる場合は、内部で200°Cを異常値として使用します。
表示側には199°C以上を0扱いにする処理があります。異常サンプルの平均・最高値の扱いの修正は[#378](https://github.com/puriso/racing_gauge/pull/378)を参照してください。

センサーの型番だけで測定精度が決まるわけではありません。
使用するセンサーの特性、基準抵抗、実際の電源電圧・配線の電圧降下に合わせて、換算係数と`VOLTAGE_DROP`を確認してください。
起動時の静止姿勢も水平Gの基準に影響します。

## 開発と検証

### ソース構成

| パス | 役割 |
| --- | --- |
| [src/main.cpp](src/main.cpp) | 初期化・フレーム制御・タッチ操作 |
| [src/modules/sensor.cpp](src/modules/sensor.cpp) | ADC・IMU取得、換算、サンプル管理 |
| [src/modules/display.cpp](src/modules/display.cpp) | ゲージ・詳細画面・最高値 |
| [src/modules/low_warning.cpp](src/modules/low_warning.cpp) | 低油圧警告・イベント情報 |
| [src/modules/racing_mode.cpp](src/modules/racing_mode.cpp) | レーシングモードの判定 |
| [src/modules/backlight.cpp](src/modules/backlight.cpp) | 周囲光による輝度制御 |
| [include/config.h](include/config.h) | 機能フラグ・表示・サンプル数などの設定 |

### CIと関連する改善

[GitHub Actions](.github/workflows/pio-build.yml)では、CoreS3向けファームウェアをビルドし、`firmware`アーティファクトとして保存します。
Markdownだけの変更は、現在のワークフローでは実行対象外です。

次の改善は関連PRで管理しています。**このREADMEの基準であるmainへの取り込み状況と、各PRの検証範囲を確認してください。**
追加テストの実行手順は各PR本文に記載しています。

| PR | 内容 |
| --- | --- |
| [#378](https://github.com/puriso/racing_gauge/pull/378) | 温度異常・最高値・油温バーの修正、nativeテストの整備 |
| [#379](https://github.com/puriso/racing_gauge/pull/379) | 更新範囲に限定した画面転送 |
| [#380](https://github.com/puriso/racing_gauge/pull/380) | 低油圧警告の再発・表示保持・時刻折り返しの修正 |
| [#381](https://github.com/puriso/racing_gauge/pull/381) | ADC通信異常時の無期限待機防止・再接続 |
| [#382](https://github.com/puriso/racing_gauge/pull/382) | DinBaseのDC給電に合わせた外部5V出力設定の統一 |

描画は最大60FPSとなるよう制御していますが、実際のフレームレートはADC取得・描画・画面転送の時間に依存します。
ビルドやホストテストの成功は、実機でのセンサー精度・電源変動・ノイズ耐性の確認とは別です。

### SDLシミュレーターとCI録画

実機なしで本番の描画を動かし、温度・油圧の変化と警告表示を確認できます。
CIではMP4・PNG・HTMLのレポートを保存します。セットアップと録画の見方は[シミュレーターのガイド](simulator/README.md)を参照してください。

## English quick start

This project displays oil pressure, water temperature, and oil temperature on an M5Stack CoreS3 using an external ADS1015.
Tap the screen to view recorded peaks and the latest low-pressure event. Records are held in RAM and reset on reboot.

### Build and upload

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html), then run:

```sh
git clone https://github.com/puriso/racing_gauge.git
cd racing_gauge
pio run -e m5stack-cores3
pio device list
```

Replace the macOS example port below with the port reported for your device:

```sh
pio run -e m5stack-cores3 -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor --port /dev/cu.usbmodemXXXX --baud 115200
```

The project currently sets `upload_port = COM11`; the command-line option overrides it.
Library versions in [platformio.ini](platformio.ini) use compatible ranges rather than exact pins.

### Hardware and operation

- **ADC inputs:** A0/CH0 oil temperature, A1/CH1 water temperature, A2/CH2 oil pressure; default address `0x48`.
- **I²C pins:** GPIO9 SDA and GPIO8 SCL. Match the 3.3V GPIO and 5V ADC logic levels; see the [wiring section](#ハードウェアと配線) and the linked device datasheets.
- **Temperature inputs:** NTC dividers with a 10kΩ resistor on the supply side and the NTC on the GND side. They are not 0.5–4.5V voltage-output sensors.
- **Power:** The intended setup uses the DinBase DC input at 12V and its built-in buck converter. [#382](https://github.com/puriso/racing_gauge/pull/382) aligns initialization and the later external-output setting using `EXTERNAL_5V_OUTPUT_ENABLED`, defaulting to `false`. For USB/battery-powered sensor output, set it to `true` and rebuild. Check the PR's merge status and verify sensor-side 5V after the change. See the [power section](#電源設定). Do not feed the 5V pin from another source while its output is enabled.
- **Sampling:** Pressure uses five samples plus smoothing. Each temperature is sampled every 500ms and averaged over the latest two samples, then smoothed; the average changes every 500ms.
- **Defaults:** Ambient-light dimming is enabled; demo mode, debug logging, and the FPS overlay are disabled. Edit [config.h](include/config.h) and rebuild to change them.
- **Calibration:** Keep the device still during startup IMU calibration. Match the NTC model and voltage-drop correction to your sensor and wiring. A zero reading can also represent a detected fault.

The sensor table above describes the firmware's conversion model, not manufacturer calibration data.
See [related improvements](#ciと関連する改善) for the temperature, rendering, warning, and ADC fixes; check each PR's merge status before relying on its behavior or test commands.
Hardware accuracy and vehicle behavior require separate verification.

## ライセンス / License

Copyright © 2025 purin. **All rights reserved.**

利用・再配布の条件は[LICENSE](LICENSE)を参照してください。MITライセンスではありません。
Use and redistribution require the author's explicit written permission under [LICENSE](LICENSE).
