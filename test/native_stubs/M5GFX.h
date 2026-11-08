#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// 実機を使わず、描画範囲と画面転送を検証するためのスタブ
namespace fonts
{
inline const int Font0 = 0;
inline const int FreeSansBold12pt7b = 1;
}  // namespace fonts
inline const int FreeSansBold24pt7b = 2;

inline unsigned long millis() { return 1000UL; }
inline float radians(float degrees) { return degrees * 0.01745329252F; }

class M5GFX
{
 public:
  void getClipRect(int32_t* x, int32_t* y, int32_t* width, int32_t* height)
  {
    *x = clipX_;
    *y = clipY_;
    *width = clipWidth_;
    *height = clipHeight_;
  }
  void setClipRect(int32_t x, int32_t y, int32_t width, int32_t height)
  {
    clipX_ = x;
    clipY_ = y;
    clipWidth_ = width;
    clipHeight_ = height;
  }

 private:
  int32_t clipX_ = 0;
  int32_t clipY_ = 0;
  int32_t clipWidth_ = 320;
  int32_t clipHeight_ = 240;
};

class M5Canvas
{
 public:
  struct Rectangle
  {
    int x;
    int y;
    int width;
    int height;
    uint16_t color;
  };

  std::vector<Rectangle> rectangles;
  std::string lastRightString;
  int pushCount = 0;

  explicit M5Canvas(M5GFX*) noexcept {}
  void fillRect(int x, int y, int width, int height, uint16_t color) { rectangles.push_back({x, y, width, height, color}); }
  void drawRightString(const char* text, int, int) { lastRightString = text; }
  void pushSprite(int, int) { ++pushCount; }
  void setFont(const int*) {}
  void setTextSize(int) {}
  void setTextColor(uint16_t, uint16_t = 0) {}
  void setCursor(int, int) {}
  void setTextFont(int) {}
  void fillScreen(uint16_t) {}
  void drawPixel(int, int, uint16_t) {}
  void drawLine(int, int, int, int, uint16_t) {}
  void drawRect(int, int, int, int, uint16_t) {}
  void fillArc(int, int, int, int, float, float, uint16_t) {}
  int textWidth(const char* text) { return static_cast<int>(std::strlen(text)) * 8; }
  int fontHeight() { return 16; }
  void print(const char*) {}
  template <typename... Args>
  void printf(const char*, Args...)
  {
  }
};
