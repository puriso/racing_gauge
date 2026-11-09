#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "../../src/modules/low_warning.h"

const char* currentGDirection = "Right";
static uint32_t nowMs = 0;
unsigned long millis() { return nowMs; }

static void expect(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

static void update(uint32_t now, float g, float pressure)
{
  nowMs = now;
  static M5Canvas canvas;
  bool changed = false;
  drawLowPressureWarning(canvas, g, pressure, changed);
}

static void historyPersists()
{
  expect(!hasLowWarningHistory(), "起動時にはW履歴がない");
  update(100, 1.2F, 2.0F);
  update(599, 1.2F, 2.0F);
  expect(!hasLowWarningHistory(), "499msでは履歴にしない");
  update(600, 1.2F, 2.0F);
  expect(hasLowWarningHistory(), "LOW成立と同時に履歴を残す");
  update(700, 0.0F, 5.0F);
  update(10000, 0.0F, 5.0F);
  expect(hasLowWarningHistory(), "LOWの保持期限後も履歴は残る");
}

static void briefDoesNotLatch()
{
  update(100, 1.2F, 2.0F);
  update(300, 0.0F, 5.0F);
  expect(!hasLowWarningHistory(), "短い低下ではWを点灯しない");
}

static void resetAndRecur()
{
  historyPersists();
  resetLowWarningHistory();
  expect(!hasLowWarningHistory(), "明示的なリセットで履歴を消す");
  update(10100, 0.0F, 5.0F);
  expect(!hasLowWarningHistory(), "LOW保持期限から履歴が復活しない");
  update(11000, 1.5F, 1.0F);
  update(11500, 1.5F, 1.0F);
  expect(hasLowWarningHistory(), "リセット後の次のLOWで再点灯する");
}

static void redrawPreservesHistory()
{
  M5Canvas canvas;
  historyPersists();
  expect(drawLowWarningHistory(canvas), "履歴の点灯時は転送が必要");
  expect(!drawLowWarningHistory(canvas), "変化がなければ転送しない");
  invalidateLowWarningHistoryIndicator();
  expect(hasLowWarningHistory(), "画面の初期化では履歴を消さない");
  expect(drawLowWarningHistory(canvas), "履歴を保持した再描画ではWを描き直す");
  resetLowWarningHistory();
  expect(drawLowWarningHistory(canvas), "リセット時はWの消去を転送する");
  expect(!drawLowWarningHistory(canvas), "消去済みなら転送しない");
}

static void activeWarningRelatches()
{
  update(100, 1.2F, 2.0F);
  update(600, 1.2F, 2.0F);
  resetLowWarningHistory();
  update(700, 1.2F, 2.0F);
  expect(hasLowWarningHistory(), "警告条件が続く間はリセット後も再点灯する");
}

int main(int argc, char** argv)
{
  struct TestCase
  {
    const char* name;
    void (*run)();
  };
  const TestCase tests[] = {{"history", historyPersists},
                            {"brief_history", briefDoesNotLatch},
                            {"reset_recur", resetAndRecur},
                            {"redraw_history", redrawPreservesHistory},
                            {"active_reset", activeWarningRelatches}};
  if (argc != 2) return 2;
  for (const auto& test : tests)
  {
    if (std::string(argv[1]) == "--list")
      std::cout << test.name << '\n';
    else if (std::string(argv[1]) == test.name)
    {
      try
      {
        test.run();
        return 0;
      }
      catch (const std::exception& e)
      {
        std::cerr << e.what() << '\n';
        return 1;
      }
    }
  }
  return std::string(argv[1]) == "--list" ? 0 : 2;
}
