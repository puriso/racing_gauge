#ifndef RISK_HISTORY_H
#define RISK_HISTORY_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "g_direction.h"

namespace risk
{
constexpr uint32_t SAMPLE_MS = 100;
constexpr size_t HALF_WINDOW = 600;
constexpr size_t WINDOW_SAMPLES = HALF_WINDOW * 2 + 1;

enum class Kind : uint8_t
{
  None,
  Drop,
  NearLow,
  Low,
};
enum SampleFlag : uint8_t
{
  PressureValid = 1,
  GValid = 2,
  OilValid = 4,
  QualifiedLow = 8,
  PressureError = 16,
  OilError = 32,
};

// 100msごとの油圧最小/最大・G最大とその方向・油温最新値。SDや動的メモリを使わない。
struct Sample
{
  uint16_t pressureMin = 0;
  uint16_t pressureMax = 0;
  uint16_t gMax = 0;
  int16_t oilLast = 0;
  uint8_t flags = 0;
  GDirection gDirection = GDirection::None;
};
static_assert(sizeof(Sample) == 10, "Direction must use the existing sample padding");

struct Input
{
  float pressure;
  float g;
  float oil;
  bool pressureValid;
  bool oilValid;
  bool qualifiedLow;
  GDirection gDirection = GDirection::None;
};

struct Event
{
  Kind kind = Kind::None;
  uint64_t occurredMs = 0;
  float pressure = 0;
  float g = 0;
  float oil = 0;
  bool oilValid = false;
  GDirection gDirection = GDirection::None;
};

class History
{
 public:
  void update(uint32_t nowMs, const Input& input);
  [[nodiscard]] auto event() const -> const Event& { return event_; }
  [[nodiscard]] auto sample(size_t index) const -> const Sample& { return selected_[index]; }
  [[nodiscard]] auto nowMs() const -> uint64_t { return elapsedMs_; }
  [[nodiscard]] auto ageMs() const -> uint64_t { return elapsedMs_ - event_.occurredMs; }
  [[nodiscard]] auto postMs() const -> uint32_t;
  [[nodiscard]] auto preMs() const -> uint32_t;
  [[nodiscard]] auto revision() const -> uint32_t { return revision_; }
  [[nodiscard]] auto hasEvent() const -> bool { return event_.kind != Kind::None; }
  [[nodiscard]] auto hasSelection() const -> bool { return selectedPoint_; }

 private:
  void advance(uint64_t tick);
  static void merge(Sample& sample, const Input& input);
  auto candidate(const Input& input) -> Kind;
  [[nodiscard]] auto better(Kind kind, const Input& input) const -> bool;
  [[nodiscard]] auto betterOverview(const Input& input) const -> bool;
  void select(Kind kind, const Input& input);
  [[nodiscard]] auto recentMaximum() const -> float;

  std::array<Sample, WINDOW_SAMPLES> history_{};
  std::array<Sample, WINDOW_SAMPLES> selected_{};
  Event event_{};
  uint64_t elapsedMs_ = 0;
  uint64_t tick_ = 0;
  uint64_t eventTick_ = 0;
  uint64_t validSince_ = 0;
  uint64_t nearSince_ = 0;
  uint64_t dropSince_ = 0;
  uint32_t lastRawMs_ = 0;
  uint32_t revision_ = 0;
  bool started_ = false;
  bool pressureContinuous_ = false;
  bool nearPending_ = false;
  bool dropPending_ = false;
  bool postComplete_ = false;
  bool selectedPoint_ = false;
};

static_assert(sizeof(History) < 25 * 1024, "Risk history must fit the fixed RAM budget");

extern History history;
auto name(Kind kind) -> const char*;
}  // namespace risk

#endif
