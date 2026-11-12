#include "risk_history.h"

#include <algorithm>
#include <cmath>

namespace risk
{
History history;

auto name(Kind kind) -> const char*
{
  switch (kind)
  {
    case Kind::Low:
      return "LOW";
    case Kind::NearLow:
      return "NEAR";
    case Kind::Drop:
      return "DROP";
    default:
      return "NONE";
  }
}

void History::advance(uint64_t tick)
{
  if (tick - tick_ >= WINDOW_SAMPLES)
  {
    history_.fill({});
  }
  else
  {
    for (uint64_t next = tick_ + 1; next <= tick; ++next)
    {
      history_[next % WINDOW_SAMPLES] = {};
    }
  }
  tick_ = tick;
}

void History::merge(Sample& sample, const Input& input)
{
  if (input.pressureValid)
  {
    auto value = static_cast<uint16_t>(std::lround(std::clamp(input.pressure, 0.0F, 655.35F) * 100.0F));
    if ((sample.flags & PressureValid) == 0)
    {
      {
        sample.pressureMin = sample.pressureMax = value;
      }
    }
    else
    {
      sample.pressureMin = std::min(sample.pressureMin, value);
      sample.pressureMax = std::max(sample.pressureMax, value);
    }
    sample.flags |= PressureValid;
  }
  else
  {
    {
      sample.flags |= PressureError;
    }
  }
  if (std::isfinite(input.g) && input.g >= 0)
  {
    auto value = static_cast<uint16_t>(std::lround(std::min(input.g, 655.35F) * 100.0F));
    if ((sample.flags & GValid) == 0 || value > sample.gMax)
    {
      sample.gMax = value;
      sample.gDirection = input.gDirection;
    }
    sample.flags |= GValid;
  }
  if (input.oilValid)
  {
    sample.oilLast = static_cast<int16_t>(std::lround(std::clamp(input.oil, -273.0F, 199.0F) * 10.0F));
    sample.flags |= OilValid;
  }
  else
  {
    {
      sample.flags |= OilError;
    }
  }
  if (input.qualifiedLow)
  {
    sample.flags |= QualifiedLow;
  }
}

auto History::recentMaximum() const -> float
{
  uint16_t maximum = 0;
  const uint64_t start = tick_ > 5 ? tick_ - 5 : 0;
  for (uint64_t tick = start; tick <= tick_; ++tick)
  {
    const auto& sample = history_[tick % WINDOW_SAMPLES];
    if ((sample.flags & PressureError) != 0 || (sample.flags & PressureValid) == 0)
    {
      return 0;
    }
    maximum = std::max(maximum, sample.pressureMax);
  }
  return maximum / 100.0F;
}

auto History::candidate(const Input& input) -> Kind
{
  if (!input.pressureValid || !std::isfinite(input.g) || input.g < 0)
  {
    pressureContinuous_ = nearPending_ = dropPending_ = false;
    return Kind::None;
  }
  if (!pressureContinuous_)
  {
    pressureContinuous_ = true;
    validSince_ = elapsedMs_;
  }
  // LOWは本番の成立状態をそのまま優先し、保持表示だけでは新しいイベントにしない。
  if (input.qualifiedLow)
  {
    return Kind::Low;
  }
  if (elapsedMs_ - validSince_ < 1000)
  {
    nearPending_ = dropPending_ = false;
    return Kind::None;
  }

  const bool near = input.g > 1.0F && input.pressure <= 3.5F;
  if (near && !nearPending_)
  {
    nearSince_ = elapsedMs_;
  }
  nearPending_ = near;
  const float baseline = recentMaximum();
  const bool drop = input.g >= 0.8F && baseline - input.pressure >= 1.5F && input.pressure <= baseline * 0.7F;
  if (drop && !dropPending_)
  {
    dropSince_ = elapsedMs_;
  }
  dropPending_ = drop;
  if (near && elapsedMs_ - nearSince_ >= 300)
  {
    return Kind::NearLow;
  }
  if (drop && elapsedMs_ - dropSince_ >= 150)
  {
    return Kind::Drop;
  }
  return Kind::None;
}

auto History::better(Kind kind, const Input& input) const -> bool
{
  if (kind == Kind::None)
  {
    return false;
  }
  if (kind != event_.kind)
  {
    return static_cast<int>(kind) > static_cast<int>(event_.kind);
  }
  // 同じ種類なら0.05bar単位の低圧を優先し、同圧域では0.1G以上強い点を採用する。
  const long pressureBand = std::lround(input.pressure * 20.0F);
  const long selectedBand = std::lround(event_.pressure * 20.0F);
  return pressureBand < selectedBand || (pressureBand == selectedBand && input.g >= event_.g + 0.1F);
}

void History::select(Kind kind, const Input& input)
{
  selectedPoint_ = true;
  event_ = {kind, elapsedMs_, input.pressure, input.g, input.oil, input.oilValid, input.gDirection};
  eventTick_ = tick_;
  postComplete_ = false;
  selected_.fill({});
  const uint64_t firstTick = tick_ > HALF_WINDOW ? tick_ - HALF_WINDOW : 0;
  const size_t count = static_cast<size_t>(tick_ - firstTick + 1);
  const size_t source = firstTick % WINDOW_SAMPLES;
  const size_t destination = HALF_WINDOW + 1 - count;
  const size_t firstCount = std::min(count, WINDOW_SAMPLES - source);
  std::copy_n(history_.begin() + source, firstCount, selected_.begin() + destination);
  std::copy_n(history_.begin(), count - firstCount, selected_.begin() + destination + firstCount);
  ++revision_;
}

void History::update(uint32_t nowMs, const Input& raw)
{
  if (!started_)
  {
    started_ = true;
  }
  else
  {
    elapsedMs_ += static_cast<uint32_t>(nowMs - lastRawMs_);
  }
  lastRawMs_ = nowMs;
  advance(elapsedMs_ / SAMPLE_MS);
  Input input = raw;
  input.pressureValid = input.pressureValid && std::isfinite(input.pressure) && input.pressure >= 0;
  input.oilValid = input.oilValid && std::isfinite(input.oil);
  merge(history_[tick_ % WINDOW_SAMPLES], input);
  if (hasSelection() && tick_ >= eventTick_ && tick_ - eventTick_ <= HALF_WINDOW)
  {
    merge(selected_[HALF_WINDOW + static_cast<size_t>(tick_ - eventTick_)], input);
    ++revision_;
  }
  if (hasSelection() && !postComplete_ && ageMs() >= HALF_WINDOW * SAMPLE_MS)
  {
    postComplete_ = true;
    ++revision_;
  }
  const auto kind = candidate(input);
  if (better(kind, input))
  {
    select(kind, input);
  }
  else if (betterOverview(input))
  {
    select(Kind::None, input);
  }
}

auto History::betterOverview(const Input& input) const -> bool
{
  // 危険点を優先し、起動直後の移動平均バッファが落ち着くまで1秒待つ。
  if (hasEvent() || !pressureContinuous_ || elapsedMs_ - validSince_ < 1000)
  {
    return false;
  }
  if (!hasSelection()) return true;
  const long gBand = std::lround(input.g * 10.0F);
  const long selectedG = std::lround(event_.g * 10.0F);
  return gBand > selectedG ||
         (gBand == selectedG && std::lround(input.pressure * 20.0F) < std::lround(event_.pressure * 20.0F));
}

auto History::postMs() const -> uint32_t
{
  return static_cast<uint32_t>(std::min<uint64_t>(elapsedMs_ - event_.occurredMs, HALF_WINDOW * SAMPLE_MS));
}

auto History::preMs() const -> uint32_t
{
  return static_cast<uint32_t>(std::min<uint64_t>(eventTick_, HALF_WINDOW) * SAMPLE_MS);
}
}  // namespace risk
