#include "node.hpp"
#include "Modules/bindings.hpp"
#include "Wrap/Helpers/lua_variant.hpp"
#include "lua.hpp"
#include <imgui.h>
#include <string>
#include <utility>
#include <vector>

namespace Engine {

auto Node::DrawUiElement() -> Error {
  ImGui::Text("Node: %s", name.c_str());
  ImGui::Text("Children: %zu", children.size());

  return Error::Success();
}

auto Node::wrap_SetPosition(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Node>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Node object");
  }

  node->transform.ReadPosition(state);
  return 0;
}

auto Node::wrap_GetPosition(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Node>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Node object");
  }

  node->transform.PushPosition(state);
  return 3;
}

auto Node::wrap_SetRotation(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Node>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Node object");
  }

  node->transform.ReadRotation(state);
  return 0;
}

auto Node::wrap_GetRotation(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Node>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Node object");
  }

  node->transform.PushRotation(state);
  return 4;
}

auto Node::wrap_SetScale(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Node>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Node object");
  }

  node->transform.ReadScale(state);
  return 0;
}

auto Node::wrap_GetScale(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Node>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Node object");
  }

  node->transform.PushScale(state);
  return 3;
}

auto Node::wrap_AddChild(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Node>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Node object");
  }

  auto result = LuaWrap::VariantFromLua<SceneObject>(state, 2);
  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to parse child object: %s",
                      result.error().message.c_str());
  }

  node->children.emplace_back(result.value());

  return 0;
}

auto Node::wrap_GetChild(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Node>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Node object");
  }

  if (lua_gettop(state) != 2) {
    return luaL_error(state, "Expected exactly one argument");
  }

  auto index = static_cast<size_t>(luaL_checkinteger(state, 2));
  if (index < 1 || index > node->children.size()) {
    return luaL_error(state, "Index out of bounds");
  }

  const auto &child = node->children[index - 1];

  auto error = LuaWrap::PushVariant(state, child);
  if (Error::IsError(error)) {
    return luaL_error(state, "Failed to push child object: %s",
                      error.message.c_str());
  }

  return 1;
}

auto Node::wrap_RemoveChild(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Node>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Node object");
  }

  if (lua_gettop(state) != 2) {
    return luaL_error(state, "Expected exactly one argument");
  }

  auto index = static_cast<size_t>(luaL_checkinteger(state, 2));
  if (index < 1 || index > node->children.size()) {
    return luaL_error(state, "Index out of bounds");
  }

  node->children.erase(node->children.begin() + static_cast<int>(index - 1));

  return 0;
}

auto Node::wrap_GetChildren(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Node>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Node object");
  }

  lua_newtable(state);
  int tableIndex = lua_gettop(state);

  for (size_t i = 0; i < node->children.size(); ++i) {
    const auto &child = node->children[i];

    auto error = LuaWrap::PushVariant(state, child);
    if (Error::IsError(error)) {
      return luaL_error(state, "Failed to push child object: %s",
                        error.message.c_str());
    }

    lua_rawseti(state, tableIndex, static_cast<int>(i + 1));
  }

  return 1;
}

auto Node::wrap_SetUserdata(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Node>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Node object");
  }

  return Selectable::SetUserdata(state, node->userdataIndex);
}

auto Node::wrap_GetUserdata(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Node>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Node object");
  }

  return Selectable::GetUserdata(state, node->userdataIndex);
}

auto Node::LoadBinding(lua_State *state) -> int {
  std::vector<std::pair<std::string, lua_CFunction>> methods = {
      {"setPosition", wrap_SetPosition}, {"getPosition", wrap_GetPosition},
      {"setRotation", wrap_SetRotation}, {"getRotation", wrap_GetRotation},
      {"setScale", wrap_SetScale},       {"getScale", wrap_GetScale},
      {"addChild", wrap_AddChild},       {"getChild", wrap_GetChild},
      {"removeChild", wrap_RemoveChild}, {"getChildren", wrap_GetChildren},
      {"setUserdata", wrap_SetUserdata}, {"getUserdata", wrap_GetUserdata},
  };

  auto binding = Bindings::LuaBoundStruct<Node>("Node");
  binding.RegisterMember<&Node::name>("Name");
  binding.Register(state, methods);

  return 0;
}

} // namespace Engine