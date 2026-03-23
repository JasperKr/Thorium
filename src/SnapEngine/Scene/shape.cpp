#include "shape.hpp"
#include "Modules/bindings.hpp"
#include "Modules/reflectBindings.hpp"
#include "Wrap/wrap.hpp"
#include <imgui.h>
#include <string>
#include <utility>
#include <vector>

namespace Engine {

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

  auto pos = Bindings::LuaDocumentingStruct::CreateType(
      "position", Bindings::BindingLuaType::Vec3);
  auto rot = Bindings::LuaDocumentingStruct::CreateType(
      "rotation", Bindings::BindingLuaType::Quaternion);
  auto scale = Bindings::LuaDocumentingStruct::CreateType(
      "scale", Bindings::BindingLuaType::Vec3);
  auto mesh = Bindings::TypeInfo{
      .name = "Mesh",
      .type = Graphics::Mesh::GetType(),
      .luaType = Bindings::BindingLuaType::Userdata,
  };

  binding.DocumentCustomMethod("setPosition", "Sets the position of the shape.",
                               {pos});
  binding.DocumentCustomMethod("getPosition", "Gets the position of the shape.",
                               {}, pos);
  binding.DocumentCustomMethod("setRotation", "Sets the rotation of the shape.",
                               {rot});
  binding.DocumentCustomMethod("getRotation", "Gets the rotation of the shape.",
                               {}, rot);
  binding.DocumentCustomMethod("setScale", "Sets the scale of the shape.",
                               {scale});
  binding.DocumentCustomMethod("getScale", "Gets the scale of the shape.", {},
                               scale);
  binding.DocumentCustomMethod("setMesh", "Sets the mesh of the shape.",
                               {mesh});
  binding.DocumentCustomMethod("getMesh", "Gets the mesh of the shape.", {},
                               mesh);
  binding.Register(state, methods);

  return 0;
}

} // namespace Engine