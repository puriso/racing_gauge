#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "../../src/modules/low_warning.h"

const char* currentGDirection = "Left";
static uint32_t nowMs = 0;
unsigned long millis() { return nowMs; }

static void expect(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

static void monitorWithoutDrawing()
{
  nowMs = 100;
  updateLowPressureWarning(2.0F, 1.0F);
  nowMs = 600;
  updateLowPressureWarning(2.0F, 1.0F);
  expect(hasLowWarningHistory(), "描画しなくても500msでW履歴を保存する");
  nowMs = 700;
  updateLowPressureWarning(0.0F, 5.0F);
  expect(lastLowEventDuration > 0.59F, "詳細画面中もイベント終了を記録する");
  expect(lastLowEventPressure == 1.0F, "最低油圧を記録する");
  M5Canvas canvas;
  bool changed = false;
  expect(drawLowPressureWarning(canvas, changed) && changed, "復帰後の初描画で保持中のLOWを表示する");
  nowMs = 3600;
  updateLowPressureWarning(0.0F, 5.0F);
  expect(!drawLowPressureWarning(canvas, changed) && changed, "非表示中も保持期限を更新する");
}

static void invalidPressureCannotQualify()
{
  nowMs = 100;
  updateLowPressureWarning(2.0F, 0.0F, false);
  nowMs = 900;
  updateLowPressureWarning(2.0F, 0.0F, false);
  expect(!hasLowWarningHistory(), "通信異常の0値ではLOWを成立させない");
  nowMs = 1000;
  updateLowPressureWarning(2.0F, 0.0F, true);
  nowMs = 1500;
  updateLowPressureWarning(2.0F, 0.0F, true);
  expect(hasLowWarningHistory(), "正常な0barはLOWを成立させる");
}

static void qualifiedStateExcludesHeldWarning()
{
  nowMs = 100;
  updateLowPressureWarning(2.0F, 1.0F);
  nowMs = 600;
  updateLowPressureWarning(2.0F, 1.0F);
  expect(isLowPressureQualified(), "成立中のLOWを記録へ渡す");
  nowMs = 700;
  updateLowPressureWarning(0.0F, 5.0F);
  M5Canvas canvas;
  bool changed = false;
  expect(!isLowPressureQualified() && drawLowPressureWarning(canvas, changed), "保持表示を新しい危険点として渡さない");
}

int main(int argc, char** argv)
{
  if (argc != 2) return 2;
  std::string name = argv[1];
  if (name == "--list")
  {
    std::cout << "hidden_monitoring\ninvalid_pressure\nqualified_state\n";
    return 0;
  }
  try
  {
    if (name == "hidden_monitoring")
      monitorWithoutDrawing();
    else if (name == "invalid_pressure")
      invalidPressureCannotQualify();
    else if (name == "qualified_state")
      qualifiedStateExcludesHeldWarning();
    else
      return 2;
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
