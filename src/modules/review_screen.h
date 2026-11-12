#ifndef REVIEW_SCREEN_H
#define REVIEW_SCREEN_H

#include <cstdint>

enum class ReviewScreen : uint8_t
{
  Gauges,
  Graph,
  Details,
};

class ReviewNavigation
{
 public:
  void tap(int x, int y)
  {
    if (screen_ == ReviewScreen::Gauges)
    {
      screen_ = ReviewScreen::Graph;
    }
    else if (screen_ == ReviewScreen::Graph && x >= 256 && x < 320 && y >= 0 && y < 24)
    {
      screen_ = ReviewScreen::Details;
    }
    else
    {
      screen_ = ReviewScreen::Gauges;
    }
  }
  [[nodiscard]] auto screen() const -> ReviewScreen { return screen_; }
  [[nodiscard]] auto reviewing() const -> bool { return screen_ != ReviewScreen::Gauges; }

 private:
  ReviewScreen screen_ = ReviewScreen::Gauges;
};

extern ReviewNavigation reviewNavigation;
#endif
