#ifndef M5GFX_H
#define M5GFX_H
class M5GFX
{
 public:
  void begin() {}
  void setBrightness(int) {}
};
class M5Canvas
{
 public:
  explicit M5Canvas(M5GFX* = nullptr) {}
  void setFont(const void*) {}
  void setTextSize(int) {}
  void setTextColor(int) {}
  void setCursor(int, int) {}
  void printf(const char*, ...) {}
  void pushSprite(int, int) {}
  void fillScreen(int) {}
  void fillRect(int, int, int, int, int) {}
  void drawRightString(const char*, int, int) {}
  void drawPixel(int, int, int) {}
  void drawLine(int, int, int, int, int) {}
};
#endif  // M5GFX_H
