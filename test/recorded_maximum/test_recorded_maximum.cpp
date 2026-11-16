#include <iostream>
#include <limits>
#include <stdexcept>

#include "recorded_maximum.h"

static void expect(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

static void unmeasured()
{
  RecordedMaximum<float> peak;
  expect(!peak.hasValue(), "正常値を取得するまでは最高値なしとする");
}

static void negativeMaximum()
{
  RecordedMaximum<float> peak;
  peak.observe(-10.0F, true);
  expect(peak.hasValue() && peak.value() == -10.0F, "負温度の初回値を0で切り捨てない");
  peak.observe(-5.0F, true);
  peak.observe(-20.0F, true);
  expect(peak.value() == -5.0F, "負温度同士でも最高値を保持する");
}

static void invalidBeforeFirstSample()
{
  RecordedMaximum<float> peak;
  peak.observe(200.0F, false);
  peak.observe(0.0F, false);
  expect(!peak.hasValue(), "起動時異常値を最高値にしない");
  peak.observe(-15.0F, true);
  expect(peak.value() == -15.0F, "異常から復帰した最初の負温度で初期化する");
}

static void invalidAfterNegativeSample()
{
  RecordedMaximum<float> peak;
  peak.observe(-10.0F, true);
  peak.observe(200.0F, false);
  peak.observe(std::numeric_limits<float>::quiet_NaN(), true);
  peak.observe(std::numeric_limits<float>::infinity(), true);
  peak.observe(-20.0F, true);
  expect(peak.hasValue() && peak.value() == -10.0F, "異常・低温復帰で負の最高値を消さない");
}

static void nonFiniteBeforeFirstSample()
{
  RecordedMaximum<float> peak;
  peak.observe(std::numeric_limits<float>::quiet_NaN(), true);
  peak.observe(-std::numeric_limits<float>::infinity(), true);
  expect(!peak.hasValue(), "非有限値では取得済みにしない");
}

static void crossingZero()
{
  RecordedMaximum<int> peak;
  peak.observe(-10, true);
  peak.observe(0, true);
  expect(peak.hasValue() && peak.value() == 0, "正常な0度を未取得と区別する");
  peak.observe(5, true);
  peak.observe(-15, true);
  expect(peak.value() == 5, "負温度から正温度への最高値更新を継続する");
}

static void independentChannels()
{
  RecordedMaximum<int> oil;
  RecordedMaximum<float> water;
  oil.observe(-8, true);
  water.observe(200.0F, false);
  expect(oil.hasValue() && !water.hasValue(), "油温・水温の取得状態を共有しない");
  water.observe(-12.5F, true);
  expect(oil.value() == -8 && water.value() == -12.5F, "各センサーの負の最高値を独立して保持する");
}

int main()
{
  struct TestCase
  {
    const char* name;
    void (*run)();
  };
  const TestCase tests[] = {{"unmeasured", unmeasured},
                            {"negative", negativeMaximum},
                            {"invalid_before", invalidBeforeFirstSample},
                            {"invalid_after", invalidAfterNegativeSample},
                            {"non_finite", nonFiniteBeforeFirstSample},
                            {"zero", crossingZero},
                            {"independent", independentChannels}};
  int failures = 0;
  for (const auto& test : tests)
  {
    try
    {
      test.run();
      std::cout << test.name << ": PASS\n";
    }
    catch (const std::exception& error)
    {
      ++failures;
      std::cerr << test.name << ": FAIL " << error.what() << '\n';
    }
  }
  return failures == 0 ? 0 : 1;
}
