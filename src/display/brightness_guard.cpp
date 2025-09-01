#include "display/brightness_guard.h"

#include "config.h"
#include "modules/display.h"

namespace
{
IVoltageProvider* g_provider = nullptr;  // 電圧取得プロバイダ
uint8_t g_currentBrightness = 0;         // 現在適用中の輝度
unsigned long g_nextRetryMs = 0;         // 再評価可能時刻
}  // namespace

namespace GuardedBrightness
{
void setVoltageProvider(IVoltageProvider* provider) { g_provider = provider; }

void apply(uint8_t requested)
{
#if RG_ENABLE_VOLTAGE_GUARD
  if (g_provider == nullptr)
  {
    static VoltageProvider defaultProvider;
    g_provider = &defaultProvider;  // デフォルト実装
  }

  unsigned long now = millis();
  if (requested > g_currentBrightness && now < g_nextRetryMs)
  {
    return;  // 再評価待ち時間中は変更しない
  }

  float vin = g_provider->readVin();
  uint8_t decided =
      decide(g_currentBrightness, requested, vin, RG_VIN_MIN_FOR_RISE_V, RG_VIN_HYSTERESIS_V, RG_BRIGHTNESS_CAP_UNDER_VIN);
  if (requested > g_currentBrightness && decided < requested)
  {
    g_nextRetryMs = now + RG_RETRY_AFTER_MS;  // 抑制した場合は次回再評価時刻を設定
  }
  else
  {
    g_nextRetryMs = now;
  }

  if (decided != g_currentBrightness)
  {
    display.setBrightness(decided);
    g_currentBrightness = decided;
  }

#ifdef RG_DEBUG_POWER
  Serial.printf("[PowerGuard] vin=%.2fV, request=%u -> apply=%u\n", vin, requested, decided);
#endif
#else
  display.setBrightness(requested);
  g_currentBrightness = requested;
#endif
}
}  // namespace GuardedBrightness
