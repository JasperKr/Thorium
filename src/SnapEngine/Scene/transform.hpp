#pragma once

#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "lua.hpp"

namespace Engine {
struct Transform {
  Math::Vec3 Position{};
  Math::Quaternion Rotation;
  Math::Vec3 Scale{1.0F, 1.0F, 1.0F};

  static auto SetPosition(lua_State *state) -> int;
  static auto GetPosition(lua_State *state) -> int;
  static auto SetRotation(lua_State *state) -> int;
  static auto GetRotation(lua_State *state) -> int;
  static auto SetScale(lua_State *state) -> int;
  static auto GetScale(lua_State *state) -> int;
  static auto SetTransform(lua_State *state) -> int;
  static auto GetTransform(lua_State *state) -> int;
};

} // namespace Engine