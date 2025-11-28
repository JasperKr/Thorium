#pragma once

#include <cstdint>
#include <vector>
namespace Timer {
const int DeltaTimeBufferSize = 100;

struct TimerFrameInfo {
  std::uint64_t lastTime = 0ULL;
  uint64_t deltaTime = 0;
  std::vector<uint64_t> deltaTimes =
      std::vector<uint64_t>(DeltaTimeBufferSize, 0);
  uint64_t averageDeltaTime = 0;

  uint32_t deltaTimeBufferIndex = 0;
  uint32_t deltaTimeBufferCount = 0;
};

auto GetFPS() -> double;
auto GetTime() -> double;
auto GetDelta() -> double;
auto GetAverageDelta() -> double;
void Sleep(double seconds, bool precise = false);
void Step();
} // namespace Timer