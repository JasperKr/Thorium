#pragma once

namespace Graphics {

inline auto GetIsCurrentlyRendering() -> bool & {
  static bool CurrentlyRendering = false;
  return CurrentlyRendering;
}

inline auto GetIsStateDirty() -> bool & {
  static bool StateDirty = true;
  return StateDirty;
}

inline auto SetDirtyState() -> void { GetIsStateDirty() = true; }

} // namespace Graphics