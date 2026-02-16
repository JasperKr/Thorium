#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
namespace Graphics {

inline auto GetIsCurrentlyRendering() -> bool & {
  static thread_local bool CurrentlyRendering = false;
  return CurrentlyRendering;
}

inline auto GetIsStateDirty() -> bool & {
  static thread_local bool StateDirty = true;
  return StateDirty;
}

inline auto SetDirtyState() -> void { GetIsStateDirty() = true; }

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline auto DefaultPixelFormat = VK_FORMAT_R8G8B8A8_UNORM;

} // namespace Graphics