#ifndef STUBS_M5GFX_H
#define STUBS_M5GFX_H

#include "Arduino.h"

// ────────────────────── フォントスタブ ──────────────────────
namespace fonts
{
struct FontStub
{
};

inline FontStub Font0;
inline FontStub FreeSansBold12pt7b;
}  // namespace fonts

struct GFXfont
{
};

inline GFXfont FreeSansBold24pt7b;

namespace m5gfx
{
enum class textdatum_t
{
  top_left,
  middle_center
};
}  // namespace m5gfx

// ────────────────────── 画面制御スタブ ──────────────────────
class M5GFX
{
public:
  void init() {}
  void initDMA() {}
  void setRotation(int) {}
  void setColorDepth(int) {}
  void setBrightness(int) {}
};

class M5Canvas
{
public:
  M5Canvas() = default;
  explicit M5Canvas(M5GFX *) {}

  void setColorDepth(int) {}
  void setTextSize(int) {}
  void setTextColor(uint16_t, uint16_t = 0) {}
  void setFont(const fonts::FontStub *) {}
  void setFont(const GFXfont *) {}
  void setTextFont(int) {}
  auto textWidth(const char *) const -> int { return 0; }
  auto fontHeight() const -> int { return 0; }
  void setCursor(int, int) {}
  void setPsram(bool) {}
  void initDMA() {}
  void createSprite(int, int) {}
  void fillRect(int, int, int, int, uint16_t) {}
  void fillArc(int, int, int, int, float, float, uint16_t) {}
  void fillScreen(uint16_t) {}
  void drawLine(int, int, int, int, uint16_t) {}
  void drawRect(int, int, int, int, uint16_t) {}
  void drawPixel(int, int, uint16_t) {}
  void print(const char *) {}
  void printf(const char *, ...) {}
  void drawRightString(const char *, int, int) {}
  void drawString(const char *, int, int) {}
  void pushSprite(int, int) {}
  void setTextDatum(m5gfx::textdatum_t) {}
};

#endif  // STUBS_M5GFX_H
