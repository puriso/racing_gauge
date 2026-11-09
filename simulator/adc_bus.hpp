#pragma once

#include <cstdint>

namespace simulator
{
// 外部装置の応答だけを注入し、期限・再接続・取得は本番コードへ任せる。
enum class AdcFault : uint8_t
{
  Healthy,
  Missing,
  Busy,
  Write,
  Status,
  Result,
  SecondResult,
};
inline AdcFault adcFault = AdcFault::Healthy;
inline uint32_t adcWaitMs = 0;
inline uint32_t adcBeginCalls = 0;
inline uint32_t adcConversions = 0;

// シナリオ時刻と外部応答を対応させる確認用データ。
// NOLINTBEGIN(readability-magic-numbers)
inline auto communicationFault(uint32_t now, bool startup) -> AdcFault
{
  if (startup)
  {
    return now < 5000 ? AdcFault::Missing : AdcFault::Healthy;
  }
  if (now < 4000 || now >= 48000 || (now / 4000) % 2 == 0)
  {
    return AdcFault::Healthy;
  }
  switch (now / 8000)
  {
    case 0:
      return AdcFault::Missing;
    case 1:
      return AdcFault::Busy;
    case 2:
      return AdcFault::Write;
    case 3:
      return AdcFault::Status;
    case 4:
      return AdcFault::Result;
    default:
      return AdcFault::SecondResult;
  }
}
// NOLINTEND(readability-magic-numbers)
}  // namespace simulator
