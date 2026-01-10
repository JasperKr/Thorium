#pragma once

namespace Graphics {

inline auto GetIsCurrentlyRendering() -> bool & {
  static bool CurrentlyRendering = false;
  return CurrentlyRendering;
}

} // namespace Graphics