#include "timer.hpp"
#include <SDL3/SDL_timer.h>
#include <cstdint>
#include <vector>

namespace Timer {

auto GetTimerInfo() -> TimerFrameInfo & {
  static TimerFrameInfo timerInfo;
  return timerInfo;
}

const double NANO_TO_SECONDS = 1.0 / 1000000000.0;
const double SECONDS_TO_NANO = 1000000000.0;

const double MILLI_TO_SECONDS = 1.0 / 1000.0;
const double SECONDS_TO_MILLI = 1000.0;

inline auto NanoToSeconds(uint64_t nanoseconds) -> double {
  return static_cast<double>(nanoseconds) * NANO_TO_SECONDS;
}

inline auto SecondsToNano(double seconds) -> uint64_t {
  return static_cast<uint64_t>(seconds * SECONDS_TO_NANO);
}

inline auto MilliToSeconds(uint64_t milliseconds) -> double {
  return static_cast<double>(milliseconds) * MILLI_TO_SECONDS;
}

inline auto SecondsToMilli(double seconds) -> uint64_t {
  return static_cast<uint64_t>(seconds * SECONDS_TO_MILLI);
}

auto GetTime() -> double { return NanoToSeconds(SDL_GetTicksNS()); }
auto GetDelta() -> double { return NanoToSeconds(GetTimerInfo().deltaTime); }
auto GetAverageDelta() -> double {
  return NanoToSeconds(GetTimerInfo().averageDeltaTime);
}
auto GetFPS() -> double {
  if (GetTimerInfo().averageDeltaTime == 0) {
    return 0.0;
  }

  return 1.0 / NanoToSeconds(GetTimerInfo().averageDeltaTime);
}
void Sleep(double seconds, bool precise) {
  auto sleepTime = SecondsToNano(seconds);

  if (precise) {
    SDL_DelayPrecise(sleepTime);
  } else {
    SDL_DelayNS(sleepTime);
  }
}

void Step() {
  uint64_t currentTime = SDL_GetTicksNS();
  TimerFrameInfo &timerInfo = GetTimerInfo();

  if (timerInfo.lastTime == 0) {
    timerInfo.lastTime = currentTime;
    timerInfo.deltaTime = 0;
    return;
  }

  timerInfo.deltaTime = currentTime - timerInfo.lastTime;
  timerInfo.lastTime = currentTime;

  // Store delta time for averaging
  timerInfo.deltaTimes.at(timerInfo.deltaTimeBufferIndex) = timerInfo.deltaTime;

  timerInfo.deltaTimeBufferIndex =
      (timerInfo.deltaTimeBufferIndex + 1) % DeltaTimeBufferSize;

  if (timerInfo.deltaTimeBufferCount < DeltaTimeBufferSize) {
    timerInfo.deltaTimeBufferCount++;
  }

  // Calculate average delta time
  uint64_t totalDelta = 0;
  for (int i = 0; i < timerInfo.deltaTimeBufferCount; i++) {
    totalDelta += timerInfo.deltaTimes[i];
  }

  timerInfo.averageDeltaTime = totalDelta / timerInfo.deltaTimeBufferCount;
}
} // namespace Timer