#include <unity.h>

// sensor.cppを直接インクルードして静的関数を利用
#include "../src/modules/sensor.cpp"

// ADC値から電圧への変換をテスト
void test_convert_adc_to_voltage()
{
  float result = convertAdcToVoltage(1023);
  // 1023はおよそ3.07Vとなる
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.07f, result);
}

// 油圧変換のテスト
void test_convert_voltage_to_oil_pressure()
{
  float result = convertVoltageToOilPressure(1.0f);
  // 電圧降下補正後は約1.32barになる
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.32f, result);

  result = convertVoltageToOilPressure(0.25f);
  // 0.5V 未満は0として扱う
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, result);

  result = convertVoltageToOilPressure(4.9f);
  // 4.9V 以上はショートエラーとして0扱い
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, result);
}

// 温度変換のテスト
void test_convert_voltage_to_temp()
{
  float result = convertVoltageToTemp(2.5f);
  // 電圧降下補正後は約23.5℃になる
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 23.5f, result);

  result = convertVoltageToTemp(1.0f);
  // 1.0V は約65.4℃になる
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 65.36f, result);
}

// 平均計算のテスト
void test_calculate_average()
{
  float data[3] = {1.0f, 2.0f, 3.0f};
  float avg = calculateAverage(data);
  // 1,2,3 の平均は2
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, avg);
}

// テスト実行
void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_convert_adc_to_voltage);
  RUN_TEST(test_convert_voltage_to_oil_pressure);
  RUN_TEST(test_convert_voltage_to_temp);
  RUN_TEST(test_calculate_average);
  UNITY_END();
}

void loop()
{
  // ループは使用しない
}
