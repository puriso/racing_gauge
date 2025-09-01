#ifndef VOLTAGE_PROVIDER_H
#define VOLTAGE_PROVIDER_H

#include <M5Unified.h>

// 電圧取得インターフェイス
class IVoltageProvider
{
 public:
  virtual ~IVoltageProvider() = default;
  virtual float readVin() = 0;  // 入力電圧を返す
};

// 実機の電圧取得クラス
class VoltageProvider : public IVoltageProvider
{
 public:
  float readVin() override;  // 移動平均済み電圧を返す
};

#endif  // VOLTAGE_PROVIDER_H
