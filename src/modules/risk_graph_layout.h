#ifndef RISK_GRAPH_LAYOUT_H
#define RISK_GRAPH_LAYOUT_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

#include "risk_history.h"

namespace risk
{
constexpr float MIN_OIL_SPAN = 6.0F;
constexpr float MIN_OIL_PADDING = 1.0F;
constexpr float OIL_PADDING_RATIO = 0.1F;
constexpr float DEFAULT_OIL_MAX = 150.0F;
constexpr uint16_t MIN_PEAK_G = 50;
constexpr size_t MIN_PEAK_GAP = 6000 / SAMPLE_MS;

struct TemperatureRange
{
  float minimum;
  float maximum;
};

inline auto oilTemperatureRange(const History& history) -> TemperatureRange
{
  float minimum = history.event().oilValid ? history.event().oil : std::numeric_limits<float>::max();
  float maximum = history.event().oilValid ? history.event().oil : std::numeric_limits<float>::lowest();
  for (size_t index = 0; index < WINDOW_SAMPLES; ++index)
  {
    const auto& sample = history.sample(index);
    if ((sample.flags & OilValid) != 0 && (sample.flags & OilError) == 0)
    {
      const float oil = static_cast<float>(sample.oilLast) / 10.0F;
      minimum = std::min(minimum, oil);
      maximum = std::max(maximum, oil);
    }
  }
  if (minimum > maximum)
  {
    return {0, DEFAULT_OIL_MAX};
  }

  // 10℃刻みの丸めを避け、記録値へ追従する。狭い変動でも最低6℃分の目盛り幅を確保する。
  const float center = (minimum + maximum) / 2;
  const float padding = std::max(MIN_OIL_PADDING, (maximum - minimum) * OIL_PADDING_RATIO);
  const float lower = std::min(minimum - padding, center - MIN_OIL_SPAN / 2);
  const float upper = std::max(maximum + padding, center + MIN_OIL_SPAN / 2);
  return {std::floor(lower), std::ceil(upper)};
}

struct GPeak
{
  size_t index = 0;
  uint16_t g = 0;
  GDirection direction = GDirection::None;
};

struct GPeaks
{
  std::array<GPeak, (WINDOW_SAMPLES / MIN_PEAK_GAP) + 1> values{};
  size_t count = 0;

  void add(const GPeak& peak)
  {
    // 6秒未満に近接する山は強い方へまとめ、文字の重なりを防ぐ。
    if (count != 0 && peak.index - values[count - 1].index < MIN_PEAK_GAP)
    {
      if (peak.g > values[count - 1].g)
      {
        values[count - 1] = peak;
      }
    }
    else if (count < values.size())
    {
      values[count++] = peak;
    }
  }
};

inline auto gPeaks(const History& history) -> GPeaks
{
  GPeaks result;
  for (size_t first = 0; first < WINDOW_SAMPLES; ++first)
  {
    const auto& sample = history.sample(first);
    // 0.5G未満の細かな揺れには文字を付けない。
    if ((sample.flags & GValid) == 0 || sample.gMax < MIN_PEAK_G)
    {
      continue;
    }
    size_t last = first;
    while (last + 1 < WINDOW_SAMPLES && (history.sample(last + 1).flags & GValid) != 0 &&
           history.sample(last + 1).gMax == sample.gMax)
    {
      ++last;
    }
    const bool rising =
        first == 0 || (history.sample(first - 1).flags & GValid) == 0 || history.sample(first - 1).gMax < sample.gMax;
    const bool falling = last + 1 == WINDOW_SAMPLES || (history.sample(last + 1).flags & GValid) == 0 ||
                         history.sample(last + 1).gMax < sample.gMax;
    const size_t center = first + ((last - first) / 2);
    first = last;
    if (!rising || !falling)
    {
      continue;
    }
    const auto direction = history.sample(center).gDirection;
    if (direction != GDirection::None)
    {
      result.add({center, sample.gMax, direction});
    }
  }
  return result;
}
}  // namespace risk

#endif
