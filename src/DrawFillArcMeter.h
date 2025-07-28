#ifndef DRAW_FILL_ARC_METER_H
#define DRAW_FILL_ARC_METER_H

#include <M5GFX.h>  // 必要なライブラリをインクルード

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

// std::clamp が利用できない環境向けの簡易版
template <typename T>
static inline T clampValue(T val, T low, T high)
{
  if (val < low) return low;
  if (val > high) return high;
  return val;
}

void drawFillArcMeter(M5Canvas /*unused*/ &canvas, float value, float minValue, float maxValue, float threshold,
                      uint16_t overThresholdColor, const char *unit, const char *label, float &maxRecordedValue,
                      float &previousValue,  // 前回描画した値
                      float tickStep,        // 目盛の間隔（細かい目盛り）
                      bool isUseDecimal,     // 小数点を表示するかどうか
                      int x, int y, bool drawStatic,
                      float majorTickStep = -1.0f,  // 数字を表示する目盛間隔（負なら旧仕様）
                      float labelStart = 0.0f)      // ラベル描画を開始する値
{
  // 左端を 1px 固定しつつ数値表示位置は従来通りに保つ
  const int GAUGE_LEFT = x + 1;                    // 円メーターの左端
  const int CENTER_X_CORRECTED = GAUGE_LEFT + 70;  // 半径 70px を考慮した中心X座標
  const int VALUE_BASE_X = x + 160;                // 数値表示位置
  const int CENTER_Y_CORRECTED = y + 90 - 10;      // スプライト内の中心Y座標
  const int RADIUS = 70;                           // 半円メーターの半径
  const int ARC_WIDTH = 10;                        // 弧の幅

  const uint16_t BACKGROUND_COLOR = COLOR_BLACK;  // 背景色
  const uint16_t ACTIVE_COLOR = COLOR_WHITE;      // 現在の値の色
  const uint16_t INACTIVE_COLOR = 0x18E3;         // メーター全体の背景色
  const uint16_t TEXT_COLOR = COLOR_WHITE;        // テキストの色

  // 温度が199℃以上ならセンサー異常として扱う
  if (strcmp(unit, "Celsius") == 0 && value >= 199.0f)
  {
    value = 0.0f;
  }

  // 値を範囲内に収める
  float clampedValue = value;
  if (clampedValue < minValue)
    clampedValue = minValue;
  else if (clampedValue > maxValue)
    clampedValue = maxValue;

  // 初回は全体を描画してキャッシュを初期化
  if (drawStatic || std::isnan(previousValue))
  {
    canvas.fillArc(CENTER_X_CORRECTED, CENTER_Y_CORRECTED, RADIUS - ARC_WIDTH, RADIUS, -270, 0, INACTIVE_COLOR);
    // 初期値を0にして次の処理でバーを全描画
    // 温度や油圧の初期表示を0とするため
    previousValue = 0.0f;
  }

  if (drawStatic)
  {
    // レッドゾーンの背景を描画
    // 背景グレーと 1px の隙間を空け常に赤で表示する
    float redZoneStartAngle = -270 + ((threshold - minValue) / (maxValue - minValue) * 270.0);
    canvas.fillArc(CENTER_X_CORRECTED, CENTER_Y_CORRECTED,
                   RADIUS - ARC_WIDTH - 9,  // 内側半径
                   RADIUS - ARC_WIDTH - 4,  // 外側半径
                   redZoneStartAngle, 0,
                   COLOR_RED);  // レッドゾーンは常に赤表示
  }

  // 前回値との比較で変更部分のみ更新
  float prevValue = std::isnan(previousValue) ? minValue : clampValue(previousValue, minValue, maxValue);
  float prevAngle = -270 + ((prevValue - minValue) / (maxValue - minValue) * 270.0);
  float currAngle = -270 + ((clampedValue - minValue) / (maxValue - minValue) * 270.0);
  float thresholdAngle = -270 + ((threshold - minValue) / (maxValue - minValue) * 270.0);

  bool prevOver = prevValue >= threshold;
  bool currOver = clampedValue >= threshold;

  if (currOver)
  {
    if (!prevOver)
    {
      // レッドゾーンに入ったのでバー全体を赤く塗り替える
      canvas.fillArc(CENTER_X_CORRECTED, CENTER_Y_CORRECTED, RADIUS - ARC_WIDTH, RADIUS, -270, currAngle,
                     overThresholdColor);
    }
    else if (currAngle > prevAngle)
    {
      // 増加分のみ赤で更新
      canvas.fillArc(CENTER_X_CORRECTED, CENTER_Y_CORRECTED, RADIUS - ARC_WIDTH, RADIUS, prevAngle, currAngle,
                     overThresholdColor);
    }
    else if (currAngle < prevAngle)
    {
      // 減少分を消去
      canvas.fillArc(CENTER_X_CORRECTED, CENTER_Y_CORRECTED, RADIUS - ARC_WIDTH, RADIUS, currAngle, prevAngle,
                     INACTIVE_COLOR);
    }
  }
  else
  {  // 閾値未満
    if (prevOver)
    {
      // レッドゾーンから戻ったので白色で描き直す
      canvas.fillArc(CENTER_X_CORRECTED, CENTER_Y_CORRECTED, RADIUS - ARC_WIDTH, RADIUS, -270, currAngle, ACTIVE_COLOR);
      if (prevAngle > currAngle)
      {
        canvas.fillArc(CENTER_X_CORRECTED, CENTER_Y_CORRECTED, RADIUS - ARC_WIDTH, RADIUS, currAngle, prevAngle,
                       INACTIVE_COLOR);
      }
    }
    else
    {
      if (currAngle > prevAngle)
      {
        canvas.fillArc(CENTER_X_CORRECTED, CENTER_Y_CORRECTED, RADIUS - ARC_WIDTH, RADIUS, prevAngle, currAngle,
                       ACTIVE_COLOR);
      }
      else if (currAngle < prevAngle)
      {
        canvas.fillArc(CENTER_X_CORRECTED, CENTER_Y_CORRECTED, RADIUS - ARC_WIDTH, RADIUS, currAngle, prevAngle,
                       INACTIVE_COLOR);
      }
    }
  }

  previousValue = clampedValue;

  if (drawStatic)
  {
    // 目盛ラベルと目盛り線を描画
    int tickCount = static_cast<int>((maxValue - minValue) / tickStep) + 1;
    for (float i = 0; i <= tickCount - 1; i += 1)
    {
      float scaledValue = minValue + (tickStep * i);
      float angle = 270 - ((270.0 / (tickCount - 1)) * i);  // 開始位置のロジックを維持
      float rad = radians(angle);

      // 主要目盛かどうかを判定（majorTickStep が負なら従来と同じ判定）
      bool isMajorTick;
      if (majorTickStep < 0)
      {
        isMajorTick = (fmod(scaledValue, 1.0f) == 0.0f);
      }
      else
      {
        float diff = fmod(scaledValue - labelStart, majorTickStep);
        isMajorTick = (scaledValue >= labelStart) && (fabsf(diff) < 0.01f || fabsf(diff - majorTickStep) < 0.01f);
      }

      // 主要目盛は長めの線、細かい目盛は短めの線を描画
      int innerRadius = isMajorTick ? (RADIUS - ARC_WIDTH - 10) : (RADIUS - ARC_WIDTH - 8);
      int outerRadius = isMajorTick ? (RADIUS - ARC_WIDTH - 5) : (RADIUS - ARC_WIDTH - 7);

      int lineX1 = CENTER_X_CORRECTED + (cosf(rad) * innerRadius);
      int lineY1 = CENTER_Y_CORRECTED - (sinf(rad) * innerRadius);
      int lineX2 = CENTER_X_CORRECTED + (cosf(rad) * outerRadius);
      int lineY2 = CENTER_Y_CORRECTED - (sinf(rad) * outerRadius);

      canvas.drawLine(lineX1, lineY1, lineX2, lineY2, COLOR_WHITE);

      bool drawLabel = isMajorTick;

      if (drawLabel)
      {
        int labelX = CENTER_X_CORRECTED + (cosf(rad) * (RADIUS - ARC_WIDTH - 15));
        int labelY = CENTER_Y_CORRECTED - (sinf(rad) * (RADIUS - ARC_WIDTH - 15));

        char labelText[6];
        snprintf(labelText, sizeof(labelText), "%.0f", scaledValue);

        canvas.setTextFont(1);
        canvas.setFont(&fonts::Font0);
        canvas.setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
        canvas.setCursor(labelX - (canvas.textWidth(labelText) / 2), labelY - 4);
        canvas.print(labelText);
      }
    }

    // 単位とメーター名を表示
    char combinedLabel[30];
    snprintf(combinedLabel, sizeof(combinedLabel), "%s / %s", label, unit);
    canvas.setFont(&fonts::Font0);
    int labelX = CENTER_X_CORRECTED;
    int labelY = CENTER_Y_CORRECTED + RADIUS + 15;
    canvas.setCursor(labelX - (canvas.textWidth(combinedLabel) / 2), labelY);
    canvas.print(combinedLabel);
  }

  // 値を右下に表示
  char valueText[10];
  if (isUseDecimal)
  {
    snprintf(valueText, sizeof(valueText), "%.1f", value);
  }
  else
  {
    snprintf(valueText, sizeof(valueText), "%.0f", round(value));
  }

  int valueX = VALUE_BASE_X;  // 数字は固定位置に表示
  int valueY = CENTER_Y_CORRECTED + RADIUS - 20;

  canvas.setFont(&FreeSansBold24pt7b);
  // 数字描画領域のみを毎回黒で塗りつぶす
  canvas.fillRect(valueX - 75, valueY - canvas.fontHeight() / 2 - 2, 75, canvas.fontHeight() + 4, BACKGROUND_COLOR);
  canvas.setCursor(valueX - canvas.textWidth(valueText), valueY - (canvas.fontHeight() / 2));
  canvas.print(valueText);
}

#endif  // DRAW_FILL_ARC_METER_H
