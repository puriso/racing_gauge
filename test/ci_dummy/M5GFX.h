#ifndef M5GFX_H
#define M5GFX_H

#include <vector>

// 簡易的な M5GFX スタブ
struct M5GFX
{
  std::vector<int> history;  // 呼び出し履歴
  void setBrightness(int level) { history.push_back(level); }
};

#endif  // M5GFX_H
