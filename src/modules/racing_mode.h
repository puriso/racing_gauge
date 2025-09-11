#pragma once

#include "config.h"

// レーシングモードの状態更新と延長判定を行う
bool updateRacingMode(unsigned long &startTime, unsigned long now, float ax, float ay, float az, float baseAx, float baseAy,
                      float baseAz);
