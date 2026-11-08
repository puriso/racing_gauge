#ifndef ADC_READER_H
#define ADC_READER_H

#include <cstdint>

auto initializeAdc() -> bool;

// 失敗時は出力値を変更しない。再接続は1秒間隔で試行する。
auto readAdcChannel(uint8_t channel, int16_t& value) -> bool;

#endif  // ADC_READER_H
