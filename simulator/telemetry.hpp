#pragma once

#include <ostream>

// 製品コードを変更せず、録画時だけ描画キャッシュと輝度設定を取り出す。
void writeDisplayTelemetry(std::ostream& output);
