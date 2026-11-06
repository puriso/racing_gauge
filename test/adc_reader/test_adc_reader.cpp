#include <Adafruit_I2CDevice.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "../../src/modules/adc_reader.h"
#include "../../src/modules/sensor.cpp"

static uint32_t clockMs = 100;

unsigned long millis() { return clockMs; }
void delay(unsigned long milliseconds) { clockMs += milliseconds; }
void delayMicroseconds(unsigned int) {}

static void expect(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

static void healthy()
{
  expect(initializeAdc(), "接続できる");
  for (uint8_t channel = 0; channel < 4; ++channel)
  {
    int16_t value = 0;
    expect(readAdcChannel(channel, value), "各チャンネルを読める");
    expect(value == fake::sample, "ダミー変換ではなく2回目の結果を返す");
    expect((fake::config & 0x7000) == 0x4000 + channel * 0x1000, "指定チャンネルを選択する");
    expect((fake::config & 0x0E00) == 0, "±6.144Vのゲインを保持する");
    expect((fake::config & 0x00E0) == 0x0080, "1600SPSを保持する");
  }
  expect(fake::conversions == 8, "チャンネルごとに2回変換する");
}

static void waitsForConversion()
{
  initializeAdc();
  fake::completeAfterPolls = 3;
  int16_t value = 0;
  expect(readAdcChannel(2, value), "変換完了まで待つ");
  expect(clockMs == 104, "2回の変換で各2ms待機する");
}

static void timeout()
{
  initializeAdc();
  fake::neverComplete = true;
  int16_t value = 777;
  expect(!readAdcChannel(2, value), "完了しない変換を失敗にする");
  expect(clockMs == 105, "変換完了のポーリングは5msで終える");
  expect(value == 777, "失敗時の出力値を変更しない");
}

static void transferErrors()
{
  initializeAdc();
  int16_t value = 777;
  fake::writeFailure = true;
  expect(!readAdcChannel(2, value), "設定の書き込み失敗を検出する");
  fake::writeFailure = false;
  clockMs += 1000;
  fake::readFailure = true;
  expect(!readAdcChannel(2, value), "完了確認の読み込み失敗を検出する");
  fake::readFailure = false;
  clockMs += 1000;
  fake::resultFailure = true;
  expect(!readAdcChannel(2, value), "結果の読み込み失敗を検出する");
  expect(value == 777, "通信失敗で不完全な値を採用しない");
}

static void unavailableAtStartup()
{
  fake::present = false;
  expect(!initializeAdc(), "未接続で初期化を失敗にする");
  int16_t value = 777;
  expect(!readAdcChannel(2, value), "初期化失敗直後は変換しない");
  clockMs = 1099;
  expect(!readAdcChannel(1, value), "再試行は999msでは行わない");
  expect(fake::beginCalls == 1, "フレームやチャンネルごとに初期化を繰り返さない");
  fake::present = true;
  clockMs = 1100;
  expect(readAdcChannel(2, value), "1秒後の再接続で取得を再開する");
  expect(fake::beginCalls == 2, "同じ装置オブジェクトで再接続する");
}

static void secondConversionFailure()
{
  initializeAdc();
  fake::resultFailureAtConversion = 2;
  int16_t value = 777;
  expect(!readAdcChannel(2, value), "ダミー変換だけ成功しても読み取り成功にしない");
  expect(value == 777, "2回目の変換に失敗したときも出力値を変更しない");
}

static void reconnectAfterFailure()
{
  initializeAdc();
  fake::neverComplete = true;
  int16_t value = 777;
  readAdcChannel(2, value);
  fake::neverComplete = false;
  expect(!readAdcChannel(0, value), "故障後は別チャンネルも保留する");
  clockMs = 1104;
  expect(!readAdcChannel(2, value), "タイムアウトから999msは再接続しない");
  clockMs = 1105;
  expect(readAdcChannel(2, value), "タイムアウトから1秒後に復帰する");
}

static void clockWrap()
{
  clockMs = UINT32_MAX - 2;
  initializeAdc();
  fake::neverComplete = true;
  int16_t value = 777;
  expect(!readAdcChannel(2, value), "時刻の折り返しを挟んでもタイムアウトする");
  expect(clockMs == 2, "折り返し前後の5msを計測する");
  clockMs = 1001;
  fake::neverComplete = false;
  expect(!readAdcChannel(2, value), "折り返し後の999msは保留する");
  clockMs = 1002;
  expect(readAdcChannel(2, value), "折り返し後の1秒で復帰する");
}

static void signedResult()
{
  initializeAdc();
  fake::sample = -1;
  int16_t value = 0;
  expect(readAdcChannel(2, value) && value == -1, "12ビット値の符号を拡張する");
}

static void invalidChannel()
{
  initializeAdc();
  int16_t value = 777;
  expect(!readAdcChannel(4, value), "不正なチャンネルを拒否する");
  expect(fake::conversions == 0 && value == 777, "装置へのアクセスや出力変更をしない");
}

static void sensorFaultAndRecovery()
{
  initializeAdc();
  acquireSensorData();
  clockMs = 300;
  for (int count = 0; count < 20; ++count) acquireSensorData();
  clockMs = 1000;
  acquireSensorData();
  float normalTemp = waterTemperatureSamples[0];
  expect(normalTemp < 199.0F, "正常な温度を取得する");
  expect(calculateAverage(oilPressureSamples) > 0.0F, "正常な油圧を取得する");
  fake::resultFailure = true;
  clockMs = 1500;
  acquireSensorData();
  expect(calculateAverage(oilPressureSamples) == 0.0F, "通信失敗後に古い油圧サンプルを残さない");
  expect(calculateAverage(waterTemperatureSamples) == 200.0F, "水温異常を正常値と平均しない");
  expect(calculateAverage(oilTemperatureSamples) == 200.0F, "油温異常を正常値と平均しない");
  fake::resultFailure = false;
  clockMs = 2500;
  acquireSensorData();
  expect(std::fabs(calculateAverage(waterTemperatureSamples) - normalTemp) < 0.001F, "復帰時は正常値から再開する");
  expect(std::fabs(calculateAverage(oilTemperatureSamples) - normalTemp) < 0.001F, "油温も正常値から再開する");
}

int main(int argc, char** argv)
{
  struct TestCase
  {
    const char* name;
    void (*run)();
  };
  const TestCase tests[] = {{"healthy", healthy},
                            {"waiting", waitsForConversion},
                            {"timeout", timeout},
                            {"transfer_errors", transferErrors},
                            {"startup", unavailableAtStartup},
                            {"second_failure", secondConversionFailure},
                            {"reconnect", reconnectAfterFailure},
                            {"clock_wrap", clockWrap},
                            {"signed", signedResult},
                            {"invalid_channel", invalidChannel},
                            {"sensor_recovery", sensorFaultAndRecovery}};
  if (argc != 2) return 2;
  for (const auto& test : tests)
  {
    if (std::string(argv[1]) == "--list")
    {
      std::cout << test.name << '\n';
    }
    else if (std::string(argv[1]) == test.name)
    {
      try
      {
        test.run();
        return 0;
      }
      catch (const std::exception& error)
      {
        std::cerr << test.name << ": " << error.what() << '\n';
        return 1;
      }
    }
  }
  return std::string(argv[1]) == "--list" ? 0 : 2;
}
