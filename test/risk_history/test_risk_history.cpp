#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "review_screen.h"
#include "risk_history.h"

using risk::Input;
using risk::Kind;
static void expect(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}
static Input reading(float pressure = 5, float g = 0, float oil = 100, bool low = false)
{
  return {pressure, g, oil, true, true, low};
}
static void feed(risk::History& history, uint32_t begin, uint32_t end, Input input)
{
  for (uint32_t time = begin; time <= end; time += 50) history.update(time, input);
}
static void nearBoundary()
{
  risk::History h;
  feed(h, 0, 1000, reading());
  h.update(1100, reading(3.4F, 1.2F));
  h.update(1399, reading(3.4F, 1.2F));
  expect(!h.hasEvent(), "予兆は299msでは成立しない");
  h.update(1400, reading(3.4F, 1.2F));
  expect(h.event().kind == Kind::NearLow, "300msでLOW直前の予兆を記録する");
}
static void rapidDrop()
{
  risk::History h;
  feed(h, 0, 2000, reading(6));
  h.update(2100, reading(4, 0.9F));
  h.update(2249, reading(4, 0.9F));
  expect(!h.hasEvent(), "149msでは急低下を確定しない");
  h.update(2250, reading(4, 0.9F));
  expect(h.event().kind == Kind::Drop, "150msの30%以上・1.5bar以上低下を記録する");
}
static void ordinaryAndTransient()
{
  risk::History h;
  for (uint32_t t = 0; t <= 20000; t += 50)
  {
    float phase = static_cast<float>(t % 4000) / 2000;
    float pressure = 3 + 3 * (phase <= 1 ? phase : 2 - phase);
    h.update(t, reading(pressure, 0.9F));
  }
  expect(!h.hasEvent(), "通常の3〜6bar変動を急低下にしない");
  h.update(20100, reading(2, 2));
  h.update(20200, reading());
  expect(!h.hasEvent(), "100msの一過性低下を予兆にしない");
}
static void rankingAndAge()
{
  risk::History h;
  feed(h, 0, 1000, reading());
  feed(h, 1100, 1500, reading(2, 2));
  expect(h.event().kind == Kind::NearLow, "低圧の予兆がある");
  h.update(1600, reading(2.5F, 1.2F, 105, true));
  expect(h.event().kind == Kind::Low, "成立LOWはより低圧の予兆より優先する");
  auto at = h.event().occurredMs;
  h.update(1700, reading(2.6F, 2, 105, true));
  expect(h.event().occurredMs == at, "高Gでも油圧が高ければ置き換えない");
  h.update(1800, reading(1.0F, 1.5F, 110, true));
  h.update(1810, reading(1.0F, 1.5F, 110, true));
  expect(h.event().occurredMs == 1800, "同じ危険度なら最初の時刻を保つ");
  h.update(62000, reading());
  expect(h.ageMs() == 60200 && h.event().oil == 110, "発生からの経過時間と油温を保持する");
}
static void completeWindow()
{
  risk::History h;
  feed(h, 0, 69900, reading(5, 0.2F, 95));
  h.update(70000, reading(1, 2, 120, true));
  feed(h, 70050, 130100, reading(5, 0.2F, 100));
  expect(h.preMs() == 60000 && h.postMs() == 60000, "前後60秒を保持する");
  expect(h.sample(0).pressureMin == 500 && h.sample(600).pressureMin == 100, "危険点を中央に置き、1分前の値を保持する");
  auto last = h.sample(1200).oilLast;
  feed(h, 130150, 300000, reading(6, 0, 70));
  expect(h.sample(1200).oilLast == last, "後1分を越えたデータを追加しない");
}
static void missingAndInvalid()
{
  risk::History h;
  auto invalid = reading(0, 2);
  invalid.pressureValid = false;
  feed(h, 0, 2000, invalid);
  expect(!h.hasEvent(), "通信失敗でLOW・予兆を捏造しない");
  h.update(2100, reading(2, 2));
  h.update(2400, reading(2, 2));
  expect(!h.hasEvent(), "再接続直後の予兆を抑制する");
  h.update(2600, reading(2, 2, 0, true));
  expect(h.hasEvent() && h.preMs() == 2600 && h.postMs() == 0, "実際のLOWと未収集の前後を扱う");
  expect(h.sample(0).flags == 0 && h.sample(1200).flags == 0, "欠けた前後を0値で補完しない");
  invalid.oilValid = false;
  h.update(2700, invalid);
  expect((h.sample(601).flags & risk::PressureError) != 0, "通信異常区間を欠損として保持する");
}
static void shortPeaksAndWrap()
{
  risk::History h;
  h.update(UINT32_MAX - 200, reading(5, 0, -15));
  h.update(UINT32_MAX - 100, reading(1, 2, -10, true));
  h.update(UINT32_MAX - 90, reading(2, 0.5F, -11));
  h.update(99, reading());
  expect(h.ageMs() == 200, "millisの32bit折り返しで経過時間が壊れない");
  expect(h.sample(600).pressureMin == 100 && h.sample(600).gMax == 200 && h.sample(600).oilLast == -110,
         "短い油圧低下とGピーク、負温度も100ms区間に残す");
}
static void interruptedNear()
{
  risk::History h;
  feed(h, 0, 1000, reading(4));
  feed(h, 1100, 1500, reading(3.4F, 1));
  expect(!h.hasEvent(), "1Gと等しい場合はNEARにしない");
  feed(h, 1600, 2000, reading(3.51F, 2));
  expect(!h.hasEvent(), "3.5barを超える場合はNEARにしない");
  h.update(2100, reading(3.4F, 2));
  h.update(2300, reading(4, 0));
  h.update(2400, reading(3.4F, 2));
  h.update(2699, reading(3.4F, 2));
  expect(!h.hasEvent(), "別々の短い低下を合算しない");
  h.update(2700, reading(3.4F, 2));
  expect(h.event().kind == Kind::NearLow, "再発から300msで成立する");
}
static void laterWorstAfterRingWrap()
{
  risk::History h;
  feed(h, 0, 69950, reading(5, 0, 90));
  h.update(70000, reading(1.5F, 2, 100, true));
  feed(h, 70050, 260000, reading(5, 0, 110));
  h.update(260050, reading(1, 2, 120, true));
  expect(h.event().occurredMs == 260050 && h.sample(0).oilLast == 1100, "長時間走行後のより危険な場面にも直前1分を付ける");
  expect(h.sample(600).pressureMin == 100 && h.postMs() == 0, "前の後1分を新しい場面へ混ぜない");
}

static void navigation()
{
  ReviewNavigation n;
  n.tap(100, 100);
  expect(n.screen() == ReviewScreen::Graph, "最初のタップでグラフを表示する");
  n.tap(280, 12);
  expect(n.screen() == ReviewScreen::Details, "詳細ボタンで従来画面へ進む");
  n.tap(100, 100);
  expect(n.screen() == ReviewScreen::Gauges, "詳細からメーターへ戻る");
  n.tap(100, 100);
  n.tap(100, 100);
  expect(!n.reviewing(), "グラフからもメーターへ戻る");
}
int main()
{
  const struct
  {
    const char* name;
    void (*run)();
  } tests[] = {{"near_boundary", nearBoundary},
               {"rapid_drop", rapidDrop},
               {"ordinary", ordinaryAndTransient},
               {"ranking", rankingAndAge},
               {"window", completeWindow},
               {"invalid", missingAndInvalid},
               {"peaks_wrap", shortPeaksAndWrap},
               {"interrupted_near", interruptedNear},
               {"later_worst", laterWorstAfterRingWrap},
               {"navigation", navigation}};
  int failed = 0;
  for (const auto& test : tests)
  {
    try
    {
      test.run();
      std::cout << test.name << ": PASS\n";
    }
    catch (const std::exception& error)
    {
      ++failed;
      std::cerr << test.name << ": FAIL " << error.what() << '\n';
    }
  }
  return failed ? 1 : 0;
}
