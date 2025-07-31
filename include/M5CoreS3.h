#ifndef M5CORES3_H
#define M5CORES3_H
#include <cstdint>

#include "M5GFX.h"
class Ltr553
{
 public:
  int getAlsValue() const { return 0; }
};
class CoreS3Class
{
 public:
  M5GFX Display;
  Ltr553 Ltr553;
  void begin(int = 0) {}
};
extern CoreS3Class CoreS3;
inline unsigned long millis() { return 0; }
#endif  // M5CORES3_H
