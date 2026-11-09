#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "../../src/modules/low_warning.h"

const char* currentGDirection = "Right";
static uint32_t nowMs = 0;

unsigned long millis() { return nowMs; }

static bool update(uint32_t time, float gForce, float pressure)
{
  nowMs = time;
  static M5Canvas canvas;
  bool changed = false;
  return drawLowPressureWarning(canvas, gForce, pressure, changed);
}

static void expect(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

static void continuousLowPressure()
{
  expect(!update(100, 1.2F, 2.0F), "開始直後は警告しない");
  expect(!update(599, 1.2F, 2.0F), "499msでは警告しない");
  expect(update(600, 1.2F, 2.0F), "500msで警告する");
  expect(update(900, 1.2F, 2.0F), "条件継続中は表示を延長する");
  expect(update(1000, 0.0F, 5.0F), "条件解除後も警告を保持する");
  expect(update(3899, 0.0F, 5.0F), "最終検知から2999msは保持する");
  expect(!update(3900, 0.0F, 5.0F), "最終検知から3000msで消える");
  expect(std::fabs(lastLowEventDuration - 0.9F) < 0.001F, "イベント継続時間を記録する");
}

static void interruptedLowPressure()
{
  expect(!update(100, 1.2F, 2.0F), "最初の検知は保留する");
  expect(!update(300, 0.0F, 5.0F), "短い低下から復帰する");
  expect(!update(400, 1.2F, 2.0F), "次の低下も開始直後は保留する");
  expect(!update(600, 1.2F, 2.0F), "別々の低下を500msとして合算しない");
  expect(!update(899, 1.2F, 2.0F), "再発から499msは保留する");
  expect(update(900, 1.2F, 2.0F), "再発から500msで警告する");
}

static void repeatedEventsDuringHold()
{
  update(100, 1.2F, 2.0F);
  update(600, 1.2F, 2.0F);
  update(700, 0.0F, 5.0F);
  update(800, 1.5F, 1.0F);
  update(1300, 1.5F, 1.0F);
  update(1400, 0.0F, 5.0F);
  expect(std::fabs(lastLowEventDuration - 0.6F) < 0.001F, "保持中の再発を別イベントとして記録する");
  expect(std::fabs(lastLowEventPressure - 1.0F) < 0.001F, "再発したイベントの最低油圧を記録する");
  expect(std::fabs(lastLowEventG - 1.5F) < 0.001F, "再発したイベントのGを記録する");
}

static void briefDipPreservesLastEvent()
{
  update(100, 1.2F, 2.0F);
  update(600, 1.2F, 2.0F);
  update(700, 0.0F, 5.0F);
  expect(update(800, 1.5F, 1.0F), "短い再発でも直前のLOW表示を保持する");
  update(900, 0.0F, 5.0F);
  expect(std::fabs(lastLowEventDuration - 0.6F) < 0.001F, "短い低下で直近の警告記録を上書きしない");
  expect(std::fabs(lastLowEventPressure - 2.0F) < 0.001F, "直近の警告の最低油圧を保持する");
}

static void initialBriefDipIsNotLogged()
{
  update(100, 1.2F, 2.0F);
  update(300, 0.0F, 5.0F);
  expect(lastLowEventDuration == 0.0F, "警告にならない短い低下を警告履歴に記録しない");
}

static void pendingEventSurvivesHoldExpiry()
{
  update(100, 1.2F, 2.0F);
  update(600, 1.2F, 2.0F);
  update(700, 0.0F, 5.0F);
  expect(update(3500, 1.5F, 1.0F), "前の警告が保持されている");
  expect(!update(3600, 1.5F, 1.0F), "前の警告は期限で消える");
  expect(update(4000, 1.5F, 1.0F), "表示消去で次のイベントの開始時刻を失わない");
}

static void startsAtZero()
{
  expect(!update(0, 1.2F, 2.0F), "0msでも開始時刻を保持する");
  expect(!update(100, 1.2F, 2.0F), "100msは保留する");
  expect(update(500, 1.2F, 2.0F), "0msから500msで警告する");
}

static void holdCrossesClockWrap()
{
  constexpr uint32_t START = UINT32_MAX - 1000;
  update(START, 1.2F, 2.0F);
  expect(update(START + 500, 1.2F, 2.0F), "折り返し前に警告する");
  expect(update(START + 600, 0.0F, 5.0F), "期限が折り返し後でも警告を保持する");
  expect(update(2498, 0.0F, 5.0F), "折り返し後も2999msまで保持する");
  expect(!update(2499, 0.0F, 5.0F), "折り返し後の3000msで消える");
}

static void debounceCrossesClockWrap()
{
  update(UINT32_MAX - 199, 1.2F, 2.0F);
  expect(!update(299, 1.2F, 2.0F), "折り返しを挟んだ499msでは警告しない");
  expect(update(300, 1.2F, 2.0F), "折り返しを挟んだ500msで警告する");
}

static void thresholds()
{
  update(100, 1.0F, 2.0F);
  expect(!update(700, 1.0F, 2.0F), "1Gと等しい場合は警告しない");
  update(800, 1.2F, 3.1F);
  expect(!update(1400, 1.2F, 3.1F), "3barより高い場合は警告しない");
  update(1500, 1.2F, 3.0F);
  expect(update(2000, 1.2F, 3.0F), "3barと等しい場合は警告する");
}

int main(int argc, char** argv)
{
  struct TestCase
  {
    const char* name;
    void (*run)();
  };
  const TestCase tests[] = {{"continuous", continuousLowPressure},
                            {"interrupted", interruptedLowPressure},
                            {"repeated", repeatedEventsDuringHold},
                            {"brief", briefDipPreservesLastEvent},
                            {"initial_brief", initialBriefDipIsNotLogged},
                            {"pending", pendingEventSurvivesHoldExpiry},
                            {"zero", startsAtZero},
                            {"hold_wrap", holdCrossesClockWrap},
                            {"debounce_wrap", debounceCrossesClockWrap},
                            {"thresholds", thresholds}};
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
