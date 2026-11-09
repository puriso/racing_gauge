#pragma once

#include <cstdint>

unsigned long millis();

namespace fonts
{
inline constexpr int FreeSansBold12pt7b = 0;
inline constexpr int Font0 = 1;
}  // namespace fonts

namespace m5gfx
{
enum class textdatum_t
{
  middle_center,
  top_left
};
}  // namespace m5gfx

// 描画装置だけを置き換え、警告判定には本番コードを使う。
class M5Canvas
{
 public:
  void setFont(const int*) {}
  void setTextSize(int) {}
  int textWidth(const char*) { return 54; }
  int fontHeight() { return 24; }
  void fillRect(int, int, int, int, uint16_t) {}
  void setTextColor(uint16_t, uint16_t) {}
  void setTextDatum(m5gfx::textdatum_t) {}
  void drawString(const char*, int, int) {}
};
