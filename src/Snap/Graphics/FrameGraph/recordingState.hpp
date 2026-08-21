#pragma once

#include "Graphics/renderState.hpp"
namespace Graphics {

// NOLINTBEGIN
thread_local RenderState::State CurrentState;
thread_local RenderState::State LastStateStorage;
thread_local RenderState::State *LastState;
// NOLINTEND

} // namespace Graphics