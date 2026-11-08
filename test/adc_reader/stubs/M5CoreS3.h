#pragma once

#include <Arduino.h>

struct TestImu
{
  void getAccelData(float* x, float* y, float* z)
  {
    *x = 0.0F;
    *y = 0.0F;
    *z = 1.0F;
  }
};

struct TestM5
{
  TestImu Imu;
};

inline TestM5 M5;
