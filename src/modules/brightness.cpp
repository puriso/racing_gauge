#include "brightness.h"

#include <M5CoreS3.h>
#include <Preferences.h>

#include "config.h"
#include "display.h"

// ────────────────────── 定数 ──────────────────────

struct ButtonArea
{
  int x;
  int y;
  int w;
  int h;
};

static const ButtonArea BTN_DEC = {20, 100, 60, 40};
static const ButtonArea BTN_INC = {100, 100, 60, 40};
static const ButtonArea BTN_OK = {60, 160, 80, 40};

static Preferences prefs;
static uint8_t currentBrightness = BRIGHTNESS_DEFAULT;
static bool menuVisible = false;

// ────────────────────── 内部関数 ──────────────────────
static auto isTouched(const ButtonArea &btn) -> bool
{
  int32_t x;
  int32_t y;
  if (M5.Touch.getTouch(&x, &y))
  {
    return (x >= btn.x && x <= btn.x + btn.w && y >= btn.y && y <= btn.y + btn.h);
  }
  return false;
}

static void drawButton(const ButtonArea &btn, const char *label)
{
  mainCanvas.drawRect(btn.x, btn.y, btn.w, btn.h, COLOR_WHITE);
  int16_t tw = mainCanvas.textWidth(label);
  int16_t th = mainCanvas.fontHeight();
  mainCanvas.setCursor(btn.x + (btn.w - tw) / 2, btn.y + (btn.h - th) / 2);
  mainCanvas.print(label);
}

// ────────────────────── 輝度設定 ──────────────────────
void setBrightness(uint8_t value)
{
  if (value < BRIGHTNESS_MIN)
  {
    value = BRIGHTNESS_MIN;
  }
  if (value > BRIGHTNESS_MAX)
  {
    value = BRIGHTNESS_MAX;
  }
  currentBrightness = value;
  ledcWrite(0, currentBrightness);
}

void saveBrightness()
{
  prefs.begin("settings", false);
  prefs.putUChar("brightness", currentBrightness);
  prefs.end();
}

void loadBrightness()
{
  prefs.begin("settings", true);
  currentBrightness = prefs.getUChar("brightness", BRIGHTNESS_DEFAULT);
  prefs.end();
  ledcSetup(0, 5000, 8);
  ledcAttachPin(38, 0);
  setBrightness(currentBrightness);
}

void drawBrightnessMenu()
{
  mainCanvas.fillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, COLOR_BLACK);
  mainCanvas.setTextColor(COLOR_WHITE);
  mainCanvas.setFont(&fonts::Font0);
  mainCanvas.setCursor(20, 40);
  mainCanvas.print("照度設定");

  char buf[24];
  snprintf(buf, sizeof(buf), "現在の照度: %d", currentBrightness);
  mainCanvas.setCursor(20, 60);
  mainCanvas.print(buf);

  drawButton(BTN_DEC, "🔽");
  drawButton(BTN_INC, "🔼");
  drawButton(BTN_OK, "✅");
  mainCanvas.pushSprite(0, 0);
}

void handleTouchMenu()
{
  if (!menuVisible)
  {
    if (M5.Touch.isEnabled() && M5.Touch.getCount())
    {
      menuVisible = true;
      drawBrightnessMenu();
    }
    return;
  }

  if (isTouched(BTN_DEC))
  {
    setBrightness(currentBrightness - 10);
    drawBrightnessMenu();
    delay(200);
    return;
  }
  if (isTouched(BTN_INC))
  {
    setBrightness(currentBrightness + 10);
    drawBrightnessMenu();
    delay(200);
    return;
  }
  if (isTouched(BTN_OK))
  {
    saveBrightness();
    menuVisible = false;
    mainCanvas.fillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, COLOR_BLACK);
  }
}
