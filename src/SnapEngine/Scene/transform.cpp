#include "transform.hpp"
#include "Modules/Math/mathTypes.hpp"
#include "Wrap/wrap.hpp"
#include "entity.hpp"
#include "lua.hpp"

namespace Engine {
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

auto Transform::SetPosition(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 4) {
    return luaL_error(state, "Expected 3 arguments for position (x, y, z)");
  }

  auto *transform = entity->try_get_mut<Transform>();
  if (transform == nullptr) {
    entity->set<Transform>({});
    transform = entity->try_get_mut<Transform>();
  }

  transform->Position.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  transform->Position.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  transform->Position.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));

  return 0;
}

auto Transform::GetPosition(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushnumber(state, transform->Position.x);
  lua_pushnumber(state, transform->Position.y);
  lua_pushnumber(state, transform->Position.z);
  return 3;
}

auto Transform::SetRotation(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 5) {
    return luaL_error(state, "Expected 4 arguments for position (x, y, z, w)");
  }

  auto *transform = entity->try_get_mut<Transform>();
  if (transform == nullptr) {
    entity->set<Transform>({});
    transform = entity->try_get_mut<Transform>();
  }

  transform->Rotation.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  transform->Rotation.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  transform->Rotation.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));
  transform->Rotation.w = static_cast<Math::Scalar>(luaL_checknumber(state, 5));

  return 0;
}

auto Transform::GetRotation(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushnumber(state, transform->Rotation.x);
  lua_pushnumber(state, transform->Rotation.y);
  lua_pushnumber(state, transform->Rotation.z);
  lua_pushnumber(state, transform->Rotation.w);
  return 4;
}

auto Transform::SetScale(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 4) {
    return luaL_error(state, "Expected 3 arguments for position (x, y, z)");
  }

  auto *transform = entity->try_get_mut<Transform>();
  if (transform == nullptr) {
    entity->set<Transform>({});
    transform = entity->try_get_mut<Transform>();
  }

  transform->Scale.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  transform->Scale.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  transform->Scale.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));

  return 0;
}

auto Transform::GetScale(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushnumber(state, transform->Scale.x);
  lua_pushnumber(state, transform->Scale.y);
  lua_pushnumber(state, transform->Scale.z);

  return 3;
}

auto Transform::SetTransform(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 11) {
    return luaL_error(state, "Expected 10 arguments for transform (x, y, z, "
                             "rotX, rotY, rotZ, rotW, scaleX, scaleY, scaleZ)");
  }

  auto *transform = entity->try_get_mut<Transform>();
  if (transform == nullptr) {
    entity->set<Transform>({});
    transform = entity->try_get_mut<Transform>();
  }

  transform->Position.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  transform->Position.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  transform->Position.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));

  transform->Rotation.x = static_cast<Math::Scalar>(luaL_checknumber(state, 5));
  transform->Rotation.y = static_cast<Math::Scalar>(luaL_checknumber(state, 6));
  transform->Rotation.z = static_cast<Math::Scalar>(luaL_checknumber(state, 7));
  transform->Rotation.w = static_cast<Math::Scalar>(luaL_checknumber(state, 8));

  transform->Scale.x = static_cast<Math::Scalar>(luaL_checknumber(state, 9));
  transform->Scale.y = static_cast<Math::Scalar>(luaL_checknumber(state, 10));
  transform->Scale.z = static_cast<Math::Scalar>(luaL_checknumber(state, 11));

  return 0;
}

auto Transform::GetTransform(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushnumber(state, transform->Position.x);
  lua_pushnumber(state, transform->Position.y);
  lua_pushnumber(state, transform->Position.z);

  lua_pushnumber(state, transform->Rotation.x);
  lua_pushnumber(state, transform->Rotation.y);
  lua_pushnumber(state, transform->Rotation.z);
  lua_pushnumber(state, transform->Rotation.w);

  lua_pushnumber(state, transform->Scale.x);
  lua_pushnumber(state, transform->Scale.y);
  lua_pushnumber(state, transform->Scale.z);

  return 10;
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
} // namespace Engine