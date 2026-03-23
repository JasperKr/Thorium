#include "model.hpp"
#include "Modules/bindings.hpp"
#include "Wrap/wrap.hpp"
#include <imgui.h>
#include <lua.h>
#include <utility>
#include <vector>

namespace Engine {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

thread_local uint64_t NextNodeUserdataIndex;
thread_local uint64_t NextModelUserdataIndex;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto Transform::PushPosition(lua_State *state) const -> void {
  lua_pushnumber(state, Position.x);
  lua_pushnumber(state, Position.y);
  lua_pushnumber(state, Position.z);
}

auto Transform::ReadPosition(lua_State *state) -> void {
  Position.x = static_cast<float>(luaL_checknumber(state, 1));
  Position.y = static_cast<float>(luaL_checknumber(state, 2));
  Position.z = static_cast<float>(luaL_checknumber(state, 3));
}

auto Transform::PushRotation(lua_State *state) const -> void {
  lua_pushnumber(state, Rotation.x);
  lua_pushnumber(state, Rotation.y);
  lua_pushnumber(state, Rotation.z);
  lua_pushnumber(state, Rotation.w);
}

auto Transform::ReadRotation(lua_State *state) -> void {
  Rotation.x = static_cast<float>(luaL_checknumber(state, 1));
  Rotation.y = static_cast<float>(luaL_checknumber(state, 2));
  Rotation.z = static_cast<float>(luaL_checknumber(state, 3));
  Rotation.w = static_cast<float>(luaL_checknumber(state, 4));
}

auto Transform::PushScale(lua_State *state) const -> void {
  lua_pushnumber(state, Scale.x);
  lua_pushnumber(state, Scale.y);
  lua_pushnumber(state, Scale.z);
}

auto Transform::ReadScale(lua_State *state) -> void {
  Scale.x = static_cast<float>(luaL_checknumber(state, 1));
  Scale.y = static_cast<float>(luaL_checknumber(state, 2));
  Scale.z = static_cast<float>(luaL_checknumber(state, 3));
}

auto Selectable::SetUserdata(lua_State *state, uint64_t &userdataIndex) -> int {
  if (lua_gettop(state) != 2) {
    return luaL_error(state, "Expected exactly one argument");
  }

  LuaWrap::SetStackToRegistry(state, "snap_node_userdata");
  lua_pushvalue(state, 2);

  if (userdataIndex == 0) {
    userdataIndex = ++NextNodeUserdataIndex;
  }

  lua_rawseti(state, -2, static_cast<int>(userdataIndex));

  return 0;
}

auto Selectable::GetUserdata(lua_State *state, uint64_t &userdataIndex) -> int {
  if (lua_gettop(state) != 1) {
    return luaL_error(state, "Expected no arguments");
  }

  if (userdataIndex == 0) {
    lua_pushnil(state);
    return 1;
  }

  LuaWrap::SetStackToRegistry(state, "snap_node_userdata");
  lua_rawgeti(state, -1, static_cast<int>(userdataIndex));
  return 1;
}

auto Model::DrawUiElement() -> Error {
  ImGui::Text("Model: %s", name.c_str());
  ImGui::Text("Shapes: %zu", shapes.size());

  return Error::Success();
}

auto Model::wrap_SetPosition(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<Model>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Invalid Model object");
  }

  model->transform.ReadPosition(state);
  return 0;
}

auto Model::wrap_GetPosition(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<Model>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Invalid Model object");
  }

  model->transform.PushPosition(state);
  return 3;
}

auto Model::wrap_SetRotation(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<Model>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Invalid Model object");
  }

  model->transform.ReadRotation(state);
  return 0;
}

auto Model::wrap_GetRotation(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<Model>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Invalid Model object");
  }

  model->transform.PushRotation(state);
  return 4;
}

auto Model::wrap_SetScale(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<Model>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Invalid Model object");
  }

  model->transform.ReadScale(state);
  return 0;
}

auto Model::wrap_GetScale(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<Model>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Invalid Model object");
  }

  model->transform.PushScale(state);
  return 3;
}

auto Model::wrap_SetUserdata(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Model>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Model object");
  }

  return Selectable::SetUserdata(state, node->userdataIndex);
}

auto Model::wrap_GetUserdata(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Model>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Model object");
  }

  return Selectable::GetUserdata(state, node->userdataIndex);
}

auto Model::LoadBinding(lua_State *state) -> int {
  std::vector<std::pair<std::string, lua_CFunction>> methods = {
      {"setPosition", wrap_SetPosition}, {"getPosition", wrap_GetPosition},
      {"setRotation", wrap_SetRotation}, {"getRotation", wrap_GetRotation},
      {"setScale", wrap_SetScale},       {"getScale", wrap_GetScale},
      {"setUserdata", wrap_SetUserdata}, {"getUserdata", wrap_GetUserdata},
  };

  auto binding = Bindings::LuaBoundStruct<Model>("Model");

  binding.RegisterMember<&Model::name>("Name");
  binding.RegisterStandardVectorMember<&Model::shapes>("Shape");

  binding.Register(state, methods);

  return 0;
}

} // namespace Engine