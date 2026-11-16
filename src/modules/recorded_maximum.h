#ifndef RECORDED_MAXIMUM_H
#define RECORDED_MAXIMUM_H

#include <cmath>

// 0を未取得の印にせず、初回の正常値から最高値を記録する。
template <typename T>
class RecordedMaximum
{
 public:
  void observe(T sample, bool valid)
  {
    if (!valid || !std::isfinite(sample))
    {
      return;
    }
    if (!hasValue_ || sample > value_)
    {
      value_ = sample;
    }
    hasValue_ = true;
  }

  [[nodiscard]] auto hasValue() const -> bool { return hasValue_; }
  [[nodiscard]] auto value() const -> T { return value_; }

 private:
  bool hasValue_ = false;
  T value_ = {};
};

#endif  // RECORDED_MAXIMUM_H
