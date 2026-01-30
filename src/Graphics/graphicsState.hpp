#pragma once

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

} // namespace Graphics