#include "brightness.h"

#include <M5CoreS3.h>
#include <Preferences.h>
#include <driver/ledc.h>

#include <algorithm>

#include "config.h"
#include "display.h"

namespace
{
Preferences prefs;
uint8_t currentBrightness = BRIGHTNESS_DEFAULT;
bool menuActive = false;
}  // namespace

// ────────────────────── 照度設定 ──────────────────────
void setBrightness(uint8_t value)
{
  currentBrightness = std::clamp<uint8_t>(value, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
  ledcWrite(BRIGHTNESS_PWM_CHANNEL, currentBrightness);
}

void saveBrightness()
{
  prefs.begin("config", false);
  prefs.putUChar("brightness", currentBrightness);
  prefs.end();
}

void loadBrightness()
{
  prefs.begin("config", true);
  currentBrightness = prefs.getUChar("brightness", BRIGHTNESS_DEFAULT);
  prefs.end();
  ledcSetup(BRIGHTNESS_PWM_CHANNEL, BRIGHTNESS_PWM_FREQ, BRIGHTNESS_PWM_RES);
  ledcAttachPin(BRIGHTNESS_PWM_PIN, BRIGHTNESS_PWM_CHANNEL);
  setBrightness(currentBrightness);
}

static void drawButtons()
{
  mainCanvas.drawRect(20, 100, 60, 40, COLOR_WHITE);
  mainCanvas.setCursor(40, 112);
  mainCanvas.print("\xE2\x96\xBC");

  mainCanvas.drawRect(100, 100, 60, 40, COLOR_WHITE);
  mainCanvas.setCursor(120, 112);
  mainCanvas.print("\xE2\x96\xB2");

  mainCanvas.drawRect(60, 160, 80, 40, COLOR_WHITE);
  mainCanvas.setCursor(90, 172);
  mainCanvas.print("OK");
}

void drawBrightnessMenu()
{
  mainCanvas.fillScreen(COLOR_BLACK);
  mainCanvas.setTextColor(COLOR_WHITE);
  mainCanvas.setFont(&fonts::Font0);
  mainCanvas.setTextSize(2);
  mainCanvas.setCursor(20, 40);
  mainCanvas.println("\xE7\x85\xA7\xE5\xBA\xA6\xE8\xA8\xAD\xE5\xAE\x9A");
  mainCanvas.setTextSize(1);
  mainCanvas.setCursor(20, 70);
  mainCanvas.printf("\xE7\x8F\xBE\xE5\x9C\xA8\xE3\x81\xAE\xE7\x85\xA7\xE5\xBA\xA6: %d", currentBrightness);
  drawButtons();
  mainCanvas.pushSprite(0, 0);
}

void handleTouchMenu()
{
  M5.update();
  auto t = M5.Touch.getDetail();
  if (!menuActive)
  {
    if (t.wasPressed())
    {
      menuActive = true;
      drawBrightnessMenu();
    }
    return;
  }

  if (t.wasPressed())
  {
    int x = t.x;
    int y = t.y;
    if (x >= 20 && x < 80 && y >= 100 && y < 140)
    {
      if (currentBrightness > BRIGHTNESS_MIN)
      {
        setBrightness(currentBrightness - 10);
      }
      drawBrightnessMenu();
    }
    else if (x >= 100 && x < 160 && y >= 100 && y < 140)
    {
      if (currentBrightness < BRIGHTNESS_MAX)
      {
        setBrightness(currentBrightness + 10);
      }
      drawBrightnessMenu();
    }
    else if (x >= 60 && x < 140 && y >= 160 && y < 200)
    {
      saveBrightness();
      menuActive = false;
      M5.Lcd.fillScreen(COLOR_BLACK);
    }
  }
}
