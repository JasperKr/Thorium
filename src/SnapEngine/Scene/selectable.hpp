#pragma once

#include "lua.hpp"
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Engine {
struct Selectable {
  std::string name;
  int32_t userdataIndex = 0;

  static std::vector<int32_t> freedUserdataIndices;
  static std::mutex userdataIndexMutex;
  static auto GetFreeUserdataIndex() -> int32_t {
    std::lock_guard lock(userdataIndexMutex);
    static int32_t currentIndex = 1;
    if (!freedUserdataIndices.empty()) {
      int32_t index = freedUserdataIndices.back();
      freedUserdataIndices.pop_back();
      return index;
    }
    return currentIndex++;
  }

  static auto FreeUserdataIndex(int32_t index) -> void {
    std::lock_guard lock(userdataIndexMutex);
    freedUserdataIndices.emplace_back(index);
  }

  static auto SetUserdata(lua_State *state) -> int;
  static auto GetUserdata(lua_State *state) -> int;

  auto OnRemove() {
    if (userdataIndex != 0) {
      FreeUserdataIndex(userdataIndex);
      userdataIndex = 0;
    }
  }
};

} // namespace Engine