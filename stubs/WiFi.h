#ifndef STUBS_WIFI_H
#define STUBS_WIFI_H

// ────────────────────── WiFi 制御スタブ ──────────────────────
constexpr int WIFI_OFF = 0;

class WiFiClass
{
public:
  void mode(int) {}
  void disconnect(bool) {}
};

inline WiFiClass WiFi;

#endif  // STUBS_WIFI_H
