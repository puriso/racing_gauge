#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace simulator
{
inline constexpr unsigned int TSUKUBA_SECONDS = 180;
inline constexpr float LAP_START = 60.0F;
inline constexpr float LAP_SECONDS = 80.0F;

struct LapPoint
{
  float seconds;
  float longitudinal;
  float lateral;
};

// コース順序を基にした80秒の合成波形。時刻・G・油温・油圧は実測ログではない。
inline constexpr std::array<LapPoint, 28> TSUKUBA_POINTS{{
    {0, -0.2F, 0},      {4, 0.95F, 0.05F},    {7, 0.25F, 1.35F},     {10, -0.3F, 0.7F},  {12, 0.2F, -0.55F},
    {15, 0.15F, 0.6F},  {19, 1.05F, 0},       {23, 0.1F, -1.4F},     {27, -0.3F, -0.4F}, {31, 0.65F, 0.6F},
    {34, 0.1F, 1.6F},   {36, 0.15F, 1.2F},    {38.5F, -0.2F, -1.8F}, {39.5F, 0, -2},     {40.5F, 0, -2},
    {42, 0.1F, -1.7F},  {45, -0.2F, -1.2F},   {48, 1.1F, -0.1F},     {51, 0.1F, 1.45F},  {55, -0.35F, 0.5F},
    {59, -0.25F, 0},    {65, -0.1F, 0},       {68, 0.9F, 0.2F},      {72, 0.05F, 1.6F},  {76, -0.2F, 1.4F},
    {79, -0.25F, 0.2F}, {79.5F, -0.2F, 0.1F}, {80, -0.2F, 0},
}};

struct LapSample
{
  float oil;
  float water;
  float pressure;
  float longitudinal;
  float lateral;
  float lapSeconds;
  const char* section;
};

inline auto lapSection(float seconds) -> const char*
{
  if (seconds < LAP_START) return "PRE-LAP";
  if (seconds > LAP_START + LAP_SECONDS) return "POST-LAP";
  const float lap = seconds - LAP_START;
  if (lap < 4) return "HOME STRAIGHT";
  if (lap < 11) return "TURN 1";
  if (lap < 18) return "S CURVES";
  if (lap < 28) return "HAIRPIN 1";
  if (lap < 37) return "DUNLOP";
  if (lap < 47) return "80R / 170R (MAX LOAD)";
  if (lap < 57) return "HAIRPIN 2";
  if (lap < 68) return "BACK STRAIGHT";
  return "FINAL / HOME";
}

inline auto tsukubaLap(float seconds, bool sustainedLow) -> LapSample
{
  const float lap = std::clamp(seconds - LAP_START, 0.0F, LAP_SECONDS);
  float longitudinal = 0, lateral = 0;
  if (seconds >= LAP_START && seconds <= LAP_START + LAP_SECONDS)
  {
    for (std::size_t index = 1; index < TSUKUBA_POINTS.size(); ++index)
    {
      const auto& after = TSUKUBA_POINTS[index];
      if (lap > after.seconds) continue;
      const auto& before = TSUKUBA_POINTS[index - 1];
      const float ratio = (lap - before.seconds) / (after.seconds - before.seconds);
      longitudinal = before.longitudinal + ratio * (after.longitudinal - before.longitudinal);
      lateral = before.lateral + ratio * (after.lateral - before.lateral);
      break;
    }
  }
  const float g = std::sqrt(longitudinal * longitudinal + lateral * lateral);
  const float oil = 120 + 20 * lap / LAP_SECONDS;
  // 通常はGに応じた小さな圧力低下。80R頂点だけ3barの警告閾値を短く割る。
  const float base = 5.8F - 0.008F * (oil - 120) + 0.15F * std::sin(seconds * 0.86F) + 0.08F * std::sin(seconds * 2.99F);
  const float normal = std::clamp(base - 0.5F * g, 3.1F, 6.0F);
  const float halfHold = sustainedLow ? 0.6F : 0.225F;
  const float distance = std::fabs(lap - 40);
  const float dip = std::clamp((halfHold + 0.1F - distance) / 0.1F, 0.0F, 1.0F);
  return {oil, 100 + 8 * lap / LAP_SECONDS, normal + dip * (2.4F - normal), longitudinal, lateral,
          lap, lapSection(seconds)};
}
}  // namespace simulator
