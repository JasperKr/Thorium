#include "model.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/bindings.hpp"
#include "Modules/object.hpp"
#include "Wrap/Helpers/lua_variant.hpp"
#include "Wrap/wrap.hpp"
#include "material.hpp"
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

auto Shape::DrawUiElement() -> Error {
  ImGui::Text("Shape: %s", name.c_str());

  return Error::Success();
}

auto Shape::wrap_SetPosition(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<Shape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Invalid Shape object");
  }

  shape->transform.ReadPosition(state);
  return 0;
}

auto Shape::wrap_GetPosition(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<Shape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Invalid Shape object");
  }

  shape->transform.PushPosition(state);
  return 3;
}

auto Shape::wrap_SetRotation(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<Shape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Invalid Shape object");
  }

  shape->transform.ReadRotation(state);
  return 0;
}

auto Shape::wrap_GetRotation(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<Shape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Invalid Shape object");
  }

  shape->transform.PushRotation(state);
  return 4;
}

auto Shape::wrap_SetScale(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<Shape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Invalid Shape object");
  }

  shape->transform.ReadScale(state);
  return 0;
}

auto Shape::wrap_GetScale(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<Shape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Invalid Shape object");
  }

  shape->transform.PushScale(state);
  return 3;
}

auto Shape::wrap_SetMesh(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<Shape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Invalid Shape object");
  }

  auto *mesh = LuaWrap::ObjectFromLua<Graphics::Mesh>(state, 2);
  if (mesh == nullptr) {
    return luaL_error(state, "Invalid Mesh object");
  }

  shape->mesh = Ref<Graphics::Mesh>(mesh);
  return 0;
}

auto Shape::wrap_GetMesh(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<Shape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Invalid Shape object");
  }

  if (shape->mesh.get() == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  LuaWrap::PushObject(state, Graphics::Mesh::GetType(), shape->mesh.get());
  return 1;
}

auto Shape::wrap_SetMaterial(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<Shape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Invalid Shape object");
  }

  auto *material = LuaWrap::ObjectFromLua<Renderer::Material>(state, 2);
  if (material == nullptr) {
    return luaL_error(state, "Invalid Material object");
  }

  shape->material = Ref<Renderer::Material>(material);
  return 0;
}

auto Shape::wrap_GetMaterial(lua_State *state) -> int {
  auto *shape = LuaWrap::ObjectFromLua<Shape>(state, 1);
  if (shape == nullptr) {
    return luaL_error(state, "Invalid Shape object");
  }

  LuaWrap::PushObject(state, Renderer::Material::GetType(),
                      shape->material.get());
  return 1;
}

auto Shape::wrap_SetUserdata(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Shape>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Shape object");
  }

  return Selectable::SetUserdata(state, node->userdataIndex);
}

auto Shape::wrap_GetUserdata(lua_State *state) -> int {
  auto *node = LuaWrap::ObjectFromLua<Shape>(state, 1);
  if (node == nullptr) {
    return luaL_error(state, "Invalid Shape object");
  }

  return Selectable::GetUserdata(state, node->userdataIndex);
}

auto Shape::LoadBinding(lua_State *state) -> int {
  std::vector<std::pair<std::string, lua_CFunction>> methods = {
      {"setPosition", wrap_SetPosition}, {"getPosition", wrap_GetPosition},
      {"setRotation", wrap_SetRotation}, {"getRotation", wrap_GetRotation},
      {"setScale", wrap_SetScale},       {"getScale", wrap_GetScale},
      {"setMesh", wrap_SetMesh},         {"getMesh", wrap_GetMesh},
      {"setMaterial", wrap_SetMaterial}, {"getMaterial", wrap_GetMaterial},
  };

  auto binding = Bindings::LuaBoundStruct<Shape>("Shape");
  binding.RegisterMember<&Shape::name>("Name");
  binding.Register(state, methods);

  return 0;
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