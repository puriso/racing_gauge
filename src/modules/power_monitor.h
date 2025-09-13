#ifndef POWER_MONITOR_H
#define POWER_MONITOR_H

// AXP2101の警告を初期化
void initPowerMonitor();
// AXP2101の警告を確認して画面に表示し停止
void checkPowerWarnings();

#endif  // POWER_MONITOR_H
