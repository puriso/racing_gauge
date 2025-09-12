#pragma once

#include "config.h"

// レーシングモードの状態更新と延長判定を行う
bool updateRacingMode(unsigned long &startTime, unsigned long now, float ax, float ay, float az, float baseAx, float baseAy,
                      float baseAz);

// 加速度が閾値を一定時間超えているかを判定し、レーシングモード開始の可否を返す
bool shouldStartRacingMode(unsigned long &overStart, unsigned long now, float ax, float ay, float az, float baseAx,
                           float baseAy, float baseAz);
