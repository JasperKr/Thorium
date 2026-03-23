#pragma once

#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "lua.hpp"

namespace Engine {
struct Transform {
  Math::Vec3 Position{};
  Math::Quaternion Rotation;
  Math::Vec3 Scale{1.0F, 1.0F, 1.0F};

  auto PushPosition(lua_State *state) const -> void;
  auto ReadPosition(lua_State *state) -> void;

  auto PushRotation(lua_State *state) const -> void;
  auto ReadRotation(lua_State *state) -> void;

  auto PushScale(lua_State *state) const -> void;
  auto ReadScale(lua_State *state) -> void;
};

} // namespace Engine