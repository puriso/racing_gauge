#include "sensor.h"

#include <M5CoreS3.h>
#include <Wire.h>

#include <algorithm>
#include <cmath>
#include <numeric>

// ────────────────────── グローバル変数 ──────────────────────
Adafruit_ADS1015 adsConverter;

float oilPressureSamples[PRESSURE_SAMPLE_SIZE] = {};
float waterTemperatureSamples[WATER_TEMP_SAMPLE_SIZE] = {};
float oilTemperatureSamples[OIL_TEMP_SAMPLE_SIZE] = {};
bool oilPressureOverVoltage = false;
float currentGForce = 0.0F;
const char *currentGDirection = "Right";
static int oilPressureIndex = 0;
static int waterTempIndex = 0;
static int oilTempIndex = 0;

// 最初の水温・油温取得かどうかのフラグ
static bool isFirstWaterTempSample = true;
static bool isFirstOilTempSample = true;

// ADC セトリング待ち時間 [us]
constexpr int ADC_SETTLING_US = 50;

// 温度サンプリング間隔 [ms]
// 500msごとに取得し、10サンプルで約5秒平均となる
constexpr int TEMP_SAMPLE_INTERVAL_MS = 500;
constexpr float SUPPLY_VOLTAGE = 5.0f;
// 電圧降下は config で設定
constexpr float CORRECTION_FACTOR = SUPPLY_VOLTAGE / (SUPPLY_VOLTAGE - VOLTAGE_DROP);
constexpr float THERMISTOR_R25 = 10000.0f;
constexpr float THERMISTOR_B_CONSTANT = 3380.0f;
constexpr float ABSOLUTE_TEMPERATURE_25 = 298.16f;  // 273.16 + 25
constexpr float SERIES_REFERENCE_RES = 10000.0f;

// ────────────────────── ユーティリティ ──────────────────────
static auto convertAdcToVoltage(int16_t rawAdc) -> float { return (rawAdc * 6.144F) / 2047.0F; }

static auto convertVoltageToOilPressure(float voltage) -> float
{
  // 4.9V 以上はショートエラーとみなし 0 扱い
  if (voltage >= 4.9F)
  {
    return 0.0F;
  }

  voltage *= CORRECTION_FACTOR;
  // 電源電圧近くまで上昇してもそのまま変換し、
  // 12bar 以上かどうかは呼び出し側で判断する

  // センサー実測式に基づき圧力へ変換
  return (voltage > 0.5F) ? 2.5F * (voltage - 0.5F) : 0.0F;
}

static auto convertVoltageToTemp(float voltage) -> float
{
  voltage *= CORRECTION_FACTOR;
  // 電源電圧より高い/等しい電圧は異常値として捨てる
  if (voltage <= 0.0F || voltage >= SUPPLY_VOLTAGE)
  {
    return 200.0F;
  }

  // 分圧式よりサーミスタ抵抗値を算出
  // R = Rref * (V / (Vcc - V))  (サーミスタがGND側の場合)
  float resistance = SERIES_REFERENCE_RES * (voltage / (SUPPLY_VOLTAGE - voltage));

  // Steinhart–Hart の簡易形 (β式)
  float kelvin =
      THERMISTOR_B_CONSTANT / (log(resistance / THERMISTOR_R25) + THERMISTOR_B_CONSTANT / ABSOLUTE_TEMPERATURE_25);

  return std::isnan(kelvin) ? 200.0F : kelvin - 273.16F;
}

// ────────────────────── ADC 読み取り ──────────────────────
static auto readAdcWithSettling(uint8_t ch) -> int16_t
{
  adsConverter.readADC_SingleEnded(ch);  // ダミー変換
  delayMicroseconds(ADC_SETTLING_US);    // セトリング待ち
  return adsConverter.readADC_SingleEnded(ch);
}

// ────────────────────── 温度読み取り ──────────────────────
// 指定チャンネルから温度を取得して摂氏に変換
static auto readTemperatureChannel(uint8_t ch) -> float
{
  int16_t raw = readAdcWithSettling(ch);
  return convertVoltageToTemp(convertAdcToVoltage(raw));
}

// ────────────────────── サンプルバッファ更新 ──────────────────────
// 初回は全要素を同じ値で埋め、その後はリングバッファ更新
template <size_t N>
static void updateSampleBuffer(float value, float (&buffer)[N], int &index, bool &first)
{
  if (first)
  {
    for (float &v : buffer)
    {
      v = value;
    }
    // 初期化直後は最初の要素から更新を再開する
    index = 0;
    first = false;
  }
  else
  {
    buffer[index] = value;
    index = (index + 1) % N;
  }
}

// ────────────────────── センサ取得 ──────────────────────
void acquireSensorData()
{
  static unsigned long lastWaterTempSampleTime = 0;
  static unsigned long lastOilTempSampleTime = 0;

  // デモモード用の変数
  // デモ用電圧とシーケンス管理変数
  static float demoVoltage = 0.0F;    // 現在のデモ電圧
  static unsigned long demoTick = 0;  // 更新タイマ
  static bool inPattern = false;      // 0→5V上昇後のパターンフェーズか
  static size_t patternIndex = 0;     // パターンインデックス
  // デモモードでの電圧変化パターン
  // 5V到達後に 5,0,5,4,3,2,1,0,1,2,3,4,5,0,0,2.5 と0.5秒ごとに変化させる
  constexpr float patternSeq[] = {5.0F, 0.0F, 5.0F, 4.0F, 3.0F, 2.0F, 1.0F, 0.0F,
                                  1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 0.0F, 0.0F, 2.5F};

  unsigned long now = millis();

  // IMU から加速度を取得
  float ax = 0.0F, ay = 0.0F, az = 0.0F;
  M5.Imu.getAccelData(&ax, &ay, &az);

  // ── 起動直後は複数サンプルからオフセットを平均化し縦軸を判定 ──
  static bool gForceOffsetInitialized = false;
  static float axOffset = 0.0F;
  static float ayOffset = 0.0F;
  static float azOffset = 0.0F;
  static int offsetSampleCount = 0;
  static int verticalAxis = 2;  // 0:X, 1:Y, 2:Z
  if (!gForceOffsetInitialized)
  {
    axOffset += ax;
    ayOffset += ay;
    azOffset += az;
    offsetSampleCount++;
    if (offsetSampleCount >= 20)
    {
      axOffset /= offsetSampleCount;
      ayOffset /= offsetSampleCount;
      azOffset /= offsetSampleCount;

      // 最大オフセットを持つ軸を縦方向とみなす
      float absOffsets[3] = {fabsf(axOffset), fabsf(ayOffset), fabsf(azOffset)};
      verticalAxis = 0;
      if (absOffsets[1] > absOffsets[verticalAxis]) verticalAxis = 1;
      if (absOffsets[2] > absOffsets[verticalAxis]) verticalAxis = 2;

      gForceOffsetInitialized = true;
    }
    else
    {
      // オフセット確定までは 0G 扱い
      currentGForce = 0.0F;
      currentGDirection = "Right";
      return;
    }
  }

  float adjX = ax - axOffset;
  float adjY = ay - ayOffset;
  float adjZ = az - azOffset;

  // 縦軸を除いた 2 軸のみで判定
  int lateralAxis = 0;       // 左右成分を持つ軸
  int longitudinalAxis = 0;  // 前後成分を持つ軸
  if (verticalAxis == 0)
  {
    lateralAxis = 1;       // Y: 左右
    longitudinalAxis = 2;  // Z: 前後
  }
  else if (verticalAxis == 1)
  {
    lateralAxis = 0;       // X: 左右
    longitudinalAxis = 2;  // Z: 前後
  }
  else  // verticalAxis == 2
  {
    lateralAxis = 1;       // Y: 左右
    longitudinalAxis = 0;  // X: 前後
  }

  float lat = (lateralAxis == 0) ? adjX : (lateralAxis == 1) ? adjY : adjZ;
  float lon = (longitudinalAxis == 0) ? adjX : (longitudinalAxis == 1) ? adjY : adjZ;
  float absLat = fabsf(lat);
  float absLon = fabsf(lon);

  // 方向判定。真横判定の範囲を広げるため比率で判定する
  constexpr float PURE_RATIO = 0.75F;  // 斜め判定のしきい値
  if (absLat >= absLon * PURE_RATIO)
  {
    // 左右方向として扱う
    currentGForce = sqrtf((lat * lat) + (lon * lon));
    currentGDirection = (lat >= 0.0F) ? "Right" : "Left";
  }
  else if (absLon >= absLat * PURE_RATIO)
  {
    // 前後方向として扱う
    currentGForce = sqrtf((lat * lat) + (lon * lon));
    currentGDirection = (lon >= 0.0F) ? "Front" : "Rear";
  }
  else
  {
    // 斜め方向
    currentGForce = sqrtf((lat * lat) + (lon * lon));
    if (lat >= 0.0F)
    {
      currentGDirection = (lon >= 0.0F) ? "Right/Front" : "Right/Rear";
    }
    else
    {
      currentGDirection = (lon >= 0.0F) ? "Left/Front" : "Left/Rear";
    }
  }

  // デモモード処理
  if (DEMO_MODE_ENABLED)
  {
    // 上昇フェーズ
    if (!inPattern)
    {
      if (now - demoTick >= 1000)
      {
        demoVoltage += 0.25F;
        if (demoVoltage >= 5.0F)
        {
          demoVoltage = 5.0F;
          inPattern = true;
          patternIndex = 0;
        }
        demoTick = now;
      }
    }
    else
    {
      // パターンフェーズ
      if (now - demoTick >= 500)
      {
        demoVoltage = patternSeq[patternIndex];
        patternIndex++;
        if (patternIndex >= sizeof(patternSeq) / sizeof(patternSeq[0]))
        {
          patternIndex = 0;
          inPattern = false;
          demoVoltage = 0.0F;
        }
        demoTick = now;
      }
    }

    float demoPressure = convertVoltageToOilPressure(demoVoltage);
    // 温度センサは電圧変化と逆の振る舞いにする
    float demoTemp = convertVoltageToTemp(SUPPLY_VOLTAGE - demoVoltage);

    oilPressureSamples[oilPressureIndex] = demoPressure;
    updateSampleBuffer(demoTemp, waterTemperatureSamples, waterTempIndex, isFirstWaterTempSample);
    updateSampleBuffer(demoTemp, oilTemperatureSamples, oilTempIndex, isFirstOilTempSample);

    Serial.printf("[DEMO] V:%.2f P:%.2f T:%.1f\n", demoVoltage, demoPressure, demoTemp);

    oilPressureIndex = (oilPressureIndex + 1) % PRESSURE_SAMPLE_SIZE;
    return;
  }

  // ── 通常センサ読み取り ──
  if (SENSOR_OIL_PRESSURE_PRESENT)
  {
    int16_t rawAdc = readAdcWithSettling(ADC_CH_OIL_PRESSURE);  // CH1: 油圧
    float voltage = convertAdcToVoltage(rawAdc);
    oilPressureOverVoltage = voltage >= 4.9F;
    float pressureValue = oilPressureOverVoltage ? 0.0F : convertVoltageToOilPressure(voltage);
    oilPressureSamples[oilPressureIndex] = pressureValue;
  }
  else
  {
    oilPressureSamples[oilPressureIndex] = 0.0F;
    oilPressureOverVoltage = false;
  }
  oilPressureIndex = (oilPressureIndex + 1) % PRESSURE_SAMPLE_SIZE;

  // 水温
  if (now - lastWaterTempSampleTime >= TEMP_SAMPLE_INTERVAL_MS)
  {
    float value = SENSOR_WATER_TEMP_PRESENT ? readTemperatureChannel(ADC_CH_WATER_TEMP) : 0.0f;
    updateSampleBuffer(value, waterTemperatureSamples, waterTempIndex, isFirstWaterTempSample);
    lastWaterTempSampleTime = now;
  }

  // 油温
  if (now - lastOilTempSampleTime >= TEMP_SAMPLE_INTERVAL_MS)
  {
    float value = SENSOR_OIL_TEMP_PRESENT ? readTemperatureChannel(ADC_CH_OIL_TEMP) : 0.0f;
    updateSampleBuffer(value, oilTemperatureSamples, oilTempIndex, isFirstOilTempSample);
    lastOilTempSampleTime = now;
  }
}
