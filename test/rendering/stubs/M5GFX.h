#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// 画素の転送漏れを検出するための描画スタブ。実フォントや弧の形状は再現しない
namespace fonts
{
inline const int Font0 = 0;
inline const int FreeSansBold12pt7b = 1;
}  // namespace fonts
inline const int FreeSansBold24pt7b = 2;
namespace m5gfx
{
enum class textdatum_t
{
  top_left,
  middle_center
};
}  // namespace m5gfx

inline unsigned long testTimeMs = 1000;
inline unsigned long millis() { return testTimeMs; }
inline float radians(float degrees) { return degrees * 0.01745329252F; }

struct TestRect
{
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

class M5GFX
{
 public:
  static constexpr int WIDTH = 320;
  static constexpr int HEIGHT = 240;
  std::array<uint16_t, WIDTH * HEIGHT> pixels{};
  TestRect clip{0, 0, WIDTH, HEIGHT};
  std::vector<TestRect> transfers;
  uint64_t transferredPixels = 0;

  void getClipRect(int32_t* x, int32_t* y, int32_t* width, int32_t* height)
  {
    *x = clip.x;
    *y = clip.y;
    *width = clip.width;
    *height = clip.height;
  }
  void setClipRect(int32_t x, int32_t y, int32_t width, int32_t height) { clip = {x, y, width, height}; }
  void clearClipRect() { clip = {0, 0, WIDTH, HEIGHT}; }
};

class M5Canvas
{
 public:
  std::array<uint16_t, M5GFX::WIDTH * M5GFX::HEIGHT> pixels{};
  explicit M5Canvas(M5GFX* parent) noexcept : parent_(parent) {}

  void pushSprite(int, int)
  {
    const auto& region = parent_->clip;
    parent_->transfers.push_back(region);
    parent_->transferredPixels += static_cast<uint64_t>(region.width) * region.height;
    for (int y = region.y; y < region.y + region.height; ++y)
    {
      for (int x = region.x; x < region.x + region.width; ++x)
      {
        parent_->pixels[y * M5GFX::WIDTH + x] = pixels[y * M5GFX::WIDTH + x];
      }
    }
  }
  void fillRect(int x, int y, int width, int height, uint16_t color)
  {
    for (int row = std::max(0, y); row < std::min(M5GFX::HEIGHT, y + height); ++row)
    {
      for (int col = std::max(0, x); col < std::min(M5GFX::WIDTH, x + width); ++col)
      {
        pixels[row * M5GFX::WIDTH + col] = color;
      }
    }
  }
  void fillScreen(uint16_t color) { pixels.fill(color); }
  void drawPixel(int x, int y, uint16_t color) { fillRect(x, y, 1, 1, color); }
  void drawLine(int x1, int y1, int x2, int y2, uint16_t color)
  {
    fillRect(std::min(x1, x2), std::min(y1, y2), std::abs(x2 - x1) + 1, std::abs(y2 - y1) + 1, color);
  }
  void drawRect(int x, int y, int width, int height, uint16_t color)
  {
    fillRect(x, y, width, 1, color);
    fillRect(x, y + height - 1, width, 1, color);
    fillRect(x, y, 1, height, color);
    fillRect(x + width - 1, y, 1, height, color);
  }
  void fillArc(int x, int y, int, int radius, float, float, uint16_t color)
  {
    fillRect(x - radius, y - radius, radius * 2 + 1, radius * 2 + 1, color);
  }
  void setFont(const int* font) { font_ = *font; }
  void setTextFont(int) {}
  void setTextSize(int) {}
  void setTextColor(uint16_t color, uint16_t = 0) { color_ = color; }
  void setCursor(int x, int y)
  {
    cursorX_ = x;
    cursorY_ = y;
  }
  void setTextDatum(m5gfx::textdatum_t datum) { datum_ = datum; }
  int fontHeight() const { return font_ == 0 ? 8 : font_ == 1 ? 24 : 48; }
  int textWidth(const char* text) const
  {
    return static_cast<int>(std::strlen(text)) * (font_ == 0 ? 6 : font_ == 1 ? 14 : 26);
  }
  void print(const char* text) { fillRect(cursorX_, cursorY_, textWidth(text), fontHeight(), color_); }
  void printf(const char* format, ...)
  {
    char text[128];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    print(text);
  }
  void drawRightString(const char* text, int x, int y)
  {
    fillRect(x - textWidth(text), y, textWidth(text), fontHeight(), color_);
  }
  void drawString(const char* text, int x, int y)
  {
    if (datum_ == m5gfx::textdatum_t::middle_center)
    {
      x -= textWidth(text) / 2;
      y -= fontHeight() / 2;
    }
    fillRect(x, y, textWidth(text), fontHeight(), color_);
  }

 private:
  M5GFX* parent_;
  int font_ = 0;
  int cursorX_ = 0;
  int cursorY_ = 0;
  uint16_t color_ = 0;
  m5gfx::textdatum_t datum_ = m5gfx::textdatum_t::top_left;
};
