#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

class FrameBenchmark
{
 public:
  static constexpr unsigned int WARMUP_FRAMES = 120;

  template <typename Input>
  void recordInput(uint32_t nowMs, const Input& input)
  {
    // 計測区間の外で入力を記録し、変更前後で同じ60Hz入力を再生したことを確認する。
    const std::array<uint32_t, 6> words{
        nowMs,
        floatBits(input.oil),
        floatBits(input.water),
        floatBits(input.pressure),
        floatBits(input.acceleration),
        static_cast<uint32_t>(input.menu),
    };
    for (uint32_t word : words)
    {
      traceHash ^= word;
      traceHash *= TRACE_MULTIPLIER;
    }
    ++traceFrames;
  }

  template <typename Render>
  auto measure(unsigned int tick, Render render) -> bool
  {
    if (tick < WARMUP_FRAMES)
    {
      return render();
    }
    const auto started = std::chrono::steady_clock::now();
    const bool menu = render();
    const auto elapsed = std::chrono::steady_clock::now() - started;
    // 時刻取得後に格納し、配列への追加や画像保存の時間を描画時間へ混ぜない。
    durationsNs.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    return menu;
  }

  void write(const std::filesystem::path& path, const std::string& scenario) const
  {
    if (durationsNs.empty() || std::filesystem::exists(path))
    {
      throw std::runtime_error("Benchmark requires measured frames and a new output file");
    }
    const auto totalNs = std::accumulate(durationsNs.begin(), durationsNs.end(), int64_t{0});
    if (totalNs <= 0)
    {
      throw std::runtime_error("Benchmark elapsed time must be positive");
    }
    std::ofstream output(path);
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output << R"({"scenario":")" << scenario << R"(","warmup_frames":)" << WARMUP_FRAMES << R"(,"trace_frames":)"
           << traceFrames << R"(,"trace_hash":")" << std::hex << traceHash << std::dec << R"(","measured_frames":)"
           << durationsNs.size() << R"(,"render_ns":)" << totalNs << "}\n";
  }

 private:
  static constexpr uint64_t TRACE_INITIAL = 14695981039346656037ULL;
  static constexpr uint64_t TRACE_MULTIPLIER = 1099511628211ULL;
  uint64_t traceHash = TRACE_INITIAL;
  uint64_t traceFrames = 0;
  std::vector<int64_t> durationsNs;

  static auto floatBits(float value) -> uint32_t
  {
    uint32_t bits = 0;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
  }
};
