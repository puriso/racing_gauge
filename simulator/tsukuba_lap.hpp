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
  float throttle;
};

// コース順序を基にした80秒の合成波形。時刻・G・油温・油圧は実測ログではない。
inline constexpr std::array<LapPoint, 38> TSUKUBA_POINTS{{
    {0, -0.25F, 0, 1},        {3, -0.35F, 0.02F, 1},      {3.1F, 0.85F, 0.04F, 0},  {4, 0.95F, 0.05F, 0},
    {7, 0.25F, 1.35F, 0.35F}, {10, -0.3F, 0.7F, 0.8F},    {12, 0.2F, -0.55F, 0.5F}, {15, 0.15F, 0.6F, 0.65F},
    {18, -0.3F, 0.1F, 1},     {18.1F, 1, 0, 0},           {19, 1.05F, 0, 0},        {23, 0.1F, -1.4F, 0.3F},
    {27, -0.3F, -0.4F, 0.9F}, {30, -0.35F, 0.1F, 1},      {30.1F, 0.65F, 0.2F, 0},  {31, 0.65F, 0.6F, 0.2F},
    {34, 0.1F, 1.6F, 0.8F},   {36, 0.15F, 1.2F, 1},       {38.5F, -0.2F, -1.8F, 1}, {39.6F, 0, -2, 1},
    {39.7F, 0.4F, -1.95F, 0}, {40.5F, 0.1F, -1.9F, 0.7F}, {42, 0.1F, -1.7F, 1},     {45, -0.2F, -1.2F, 1},
    {47, -0.3F, -0.3F, 1},    {47.1F, 1.1F, -0.1F, 0},    {48, 1.1F, -0.1F, 0},     {51, 0.1F, 1.45F, 0.35F},
    {55, -0.35F, 0.5F, 1},    {59, -0.35F, 0.03F, 1},     {65, -0.2F, 0.01F, 1},    {67.5F, -0.25F, 0.04F, 1},
    {67.6F, 0.9F, 0.2F, 0},   {68, 0.9F, 0.2F, 0},        {72, 0.05F, 1.6F, 0.5F},  {76, -0.2F, 1.4F, 0.9F},
    {79, -0.25F, 0.2F, 1},    {80, -0.25F, 0, 1},
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
  float throttle;
  int lapNumber;
};

inline auto lapPhase(float seconds) -> float { return std::fmod(seconds + LAP_SECONDS - LAP_START, LAP_SECONDS); }

inline auto throttleArea(float lap) -> float
{
  float area = 0;
  for (std::size_t index = 1; index < TSUKUBA_POINTS.size(); ++index)
  {
    const auto& a = TSUKUBA_POINTS[index - 1];
    const auto& b = TSUKUBA_POINTS[index];
    const float span = std::clamp(lap - a.seconds, 0.0F, b.seconds - a.seconds);
    const float end = a.throttle + (b.throttle - a.throttle) * span / (b.seconds - a.seconds);
    area += span * (a.throttle + end) / 2;
  }
  return area;
}

inline auto heatProgress(float seconds) -> float
{
  const float offset = LAP_SECONDS - LAP_START;
  const float absolute = seconds + offset;
  const float area = std::floor(absolute / LAP_SECONDS) * throttleArea(LAP_SECONDS) +
                     throttleArea(std::fmod(absolute, LAP_SECONDS)) - throttleArea(offset);
  return seconds + 2 * area;
}

inline auto lapSection(float seconds) -> const char*
{
  if (seconds < 0.8F) return "IMU SETTLING";
  const float lap = lapPhase(seconds);
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
  const float lap = lapPhase(seconds);
  float longitudinal = 0, lateral = 0, throttle = 0;
  for (std::size_t index = 1; index < TSUKUBA_POINTS.size(); ++index)
  {
    const auto& after = TSUKUBA_POINTS[index];
    if (lap > after.seconds) continue;
    const auto& before = TSUKUBA_POINTS[index - 1];
    const float ratio = (lap - before.seconds) / (after.seconds - before.seconds);
    longitudinal = before.longitudinal + ratio * (after.longitudinal - before.longitudinal);
    lateral = before.lateral + ratio * (after.lateral - before.lateral);
    throttle = before.throttle + ratio * (after.throttle - before.throttle);
    break;
  }
  // 起動補正後は前後の周回も動かし、直線中にも小さな加減速・横G変化を残す。
  const float moving = std::clamp((seconds - 0.8F) / 0.4F, 0.0F, 1.0F);
  longitudinal = moving * (longitudinal + 0.02F * std::sin(seconds * 7.1F));
  lateral = moving * (lateral + 0.025F * std::sin(seconds * 5.3F));
  float g = std::hypot(longitudinal, lateral);
  const float limit = 2.0F - 0.012F * (0.5F + 0.5F * std::sin(seconds * 6.2F));
  if (g > limit)
  {
    longitudinal *= limit / g;
    lateral *= limit / g;
    g = limit;
  }
  // 全180秒で120→140℃。アクセルが開いている区間ほど上昇を速くする。
  const float heat = heatProgress(seconds) / heatProgress(TSUKUBA_SECONDS);
  const float oil = 120 + 20 * heat;
  // 全開→オフで基準油圧が約2bar落ち、G・温度の小さな影響も重ねる。
  const float normal = 4 + 2 * throttle - 0.15F * g - 0.003F * (oil - 120) + 0.03F * std::sin(seconds * 1.4F);
  const float plateauEnd = sustainedLow ? 40.7F : 40.0F;
  const float entering = (lap - 39.72F) / 0.08F;
  const float leaving = (plateauEnd + 0.08F - lap) / 0.08F;
  const float dip =
      seconds >= LAP_START && seconds < LAP_START + LAP_SECONDS ? std::clamp(std::min(entering, leaving), 0.0F, 1.0F) : 0;
  return {oil,
          100 + 8 * heat,
          normal + dip * (2.6F - normal),
          longitudinal,
          lateral,
          lap,
          lapSection(seconds),
          throttle,
          static_cast<int>(std::floor((seconds - LAP_START) / LAP_SECONDS)) + 1};
}
}  // namespace simulator
