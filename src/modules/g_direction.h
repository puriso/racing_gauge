#ifndef G_DIRECTION_H
#define G_DIRECTION_H

#include <cmath>
#include <cstdint>

enum class GDirection : uint8_t
{
  None,
  Front,
  Right,
  Left,
  Back,
};

inline auto cardinalGDirection(float lateral, float longitudinal) -> GDirection
{
  if (!std::isfinite(lateral) || !std::isfinite(longitudinal) || (lateral == 0 && longitudinal == 0))
  {
    return GDirection::None;
  }
  if (std::fabs(longitudinal) >= std::fabs(lateral))
  {
    return longitudinal >= 0 ? GDirection::Front : GDirection::Back;
  }
  return lateral >= 0 ? GDirection::Right : GDirection::Left;
}

inline auto gDirectionLabel(GDirection direction) -> const char*
{
  switch (direction)
  {
    case GDirection::Front:
      return "F";
    case GDirection::Right:
      return "R";
    case GDirection::Left:
      return "L";
    case GDirection::Back:
      return "B";
    default:
      return "";
  }
}

#endif
