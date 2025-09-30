#ifndef STUBS_WIRE_H
#define STUBS_WIRE_H

// ────────────────────── I2C 通信スタブ ──────────────────────
class TwoWire
{
public:
  void begin(int, int) {}
};

inline TwoWire Wire;

#endif  // STUBS_WIRE_H
