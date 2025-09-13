#ifndef M5CORES3_H
#define M5CORES3_H

#include <cstdint>

// AXP2101に対するスタブクラス
struct MockAXP2101
{
  uint64_t irqMask = 0;      // 設定されたIRQマスク
  uint64_t irqStatus = 0;    // 取得するIRQステータス
  bool clearCalled = false;  // クリアが呼び出されたか

  void enableIRQ(uint64_t mask) { irqMask = mask; }
  void clearIRQStatuses() { clearCalled = true; }
  uint64_t getIRQStatuses() { return irqStatus; }
};

// 電源周りのスタブ
struct MockPower
{
  MockAXP2101 Axp2101;
};

// 画面制御のスタブ
struct MockLcd
{
  bool fillCalled = false;     // 画面塗りつぶしが呼ばれたか
  bool printlnCalled = false;  // 文字表示が呼ばれたか
  uint16_t lastFillColor = 0;  // 最後に塗りつぶした色

  void fillScreen(uint16_t color)
  {
    fillCalled = true;
    lastFillColor = color;
  }
  void setCursor(int, int) {}
  void setTextColor(uint16_t) {}
  void println(const char*) { printlnCalled = true; }
};

// M5本体のスタブ
struct MockM5
{
  MockPower Power;
  MockLcd Lcd;
};

extern MockM5 M5;  // グローバルインスタンス

// Arduino互換のdelay関数スタブ
inline void delay(uint16_t) {}

// AXP2101のIRQビット定義
namespace m5
{
constexpr uint64_t AXP2101_IRQ_LDO_OVER_CURR = 1ULL << 0;
constexpr uint64_t AXP2101_IRQ_BATFET_OVER_CURR = 1ULL << 1;
constexpr uint64_t AXP2101_IRQ_BAT_OVER_VOLTAGE = 1ULL << 2;
constexpr uint64_t AXP2101_IRQ_WARNING_LEVEL1 = 1ULL << 3;
constexpr uint64_t AXP2101_IRQ_WARNING_LEVEL2 = 1ULL << 4;
}  // namespace m5

#endif  // M5CORES3_H
