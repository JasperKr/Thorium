#include "scene.hpp"
#include "Modules/bindings.hpp"
#include "Modules/error.hpp"
#include "Modules/reflectBindings.hpp"
#include "Wrap/Helpers/lua_vector.hpp"
#include "Wrap/wrap.hpp"
#include "model.hpp"
#include "node.hpp"
#include "shape.hpp"

#include "Wrap/Helpers/lua_variant.hpp"
#include <imgui.h>
#include <utility>
#include <vector>

#include "lua.hpp"

namespace Engine {

auto Scene::LoadBinding(lua_State *state) -> int {
  const std::vector<std::pair<std::string, lua_CFunction>> methods = {
      {"getHierarchyObjects", GetHierarchyObjects},
      {"addHierarchyObject", AddHierarchyObject},
      {"getHierarchyObject", GetHierarchyObject},
      {"removeHierarchyObject", RemoveHierarchyObject},
      {"getHierarchyObjectCount", GetHierarchyObjectCount},
      {"drawUiElement", GetDrawUiElementLuaBinding<Scene>()},
  };

  const auto &type = Bindings::DefineLuaTypeAlias(
      "SceneObject", {Bindings::TypeInfo{
                          .name = "Model",
                          .type = Model::GetType(),
                          .luaType = Bindings::BindingLuaType::Userdata,
                          .isEnum = false,
                          .isVector = false,
                      },
                      Bindings::TypeInfo{
                          .name = "Node",
                          .type = Node::GetType(),
                          .luaType = Bindings::BindingLuaType::Userdata,
                          .isEnum = false,
                          .isVector = false,
                      },
                      Bindings::TypeInfo{
                          .name = "Shape",
                          .type = Shape::GetType(),
                          .luaType = Bindings::BindingLuaType::Userdata,
                          .isEnum = false,
                          .isVector = false,
                      }});

  auto binding = Bindings::LuaBoundStruct<Scene>("Scene");
  binding.RegisterMember<&Scene::name>("Name");

  binding.DocumentCustomMethod(
      "getHierarchyObjects", "Get the objects in the scene's hierarchy", {},
      Bindings::TypeInfo{
          .name = "HierarchyObjects",
          .type = &type,
          .luaType = Bindings::BindingLuaType::Userdata,
          .isVector = true,
      });

  binding.DocumentCustomMethod(
      "addHierarchyObject", "Add an object to the scene's hierarchy",
      {Bindings::TypeInfo{
          .name = "Object",
          .type = &type,
          .luaType = Bindings::BindingLuaType::Userdata,
      }},
      std::nullopt);

  binding.DocumentCustomMethod(
      "getHierarchyObject", "Get an object from the scene's hierarchy by index",
      {Bindings::LuaDocumentingStruct::CreateType(
          "Index", Bindings::BindingLuaType::Integer)},
      Bindings::TypeInfo{
          .name = "HierarchyObject",
          .type = &type,
          .luaType = Bindings::BindingLuaType::Userdata,
      });

  binding.DocumentCustomMethod(
      "removeHierarchyObject",
      "Remove an object from the scene's hierarchy by index",
      {Bindings::LuaDocumentingStruct::CreateType(
          "Index", Bindings::BindingLuaType::Integer)});
  binding.DocumentCustomMethod(
      "getHierarchyObjectCount",
      "Get the number of objects in the scene's hierarchy", {},
      Bindings::LuaDocumentingStruct::CreateType(
          "Count", Bindings::BindingLuaType::Integer));

  binding.Register(state, methods);

  return 1;
}

auto Scene::GetHierarchyObjects(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);
  if (scene == nullptr) {
    return luaL_error(state, "Invalid scene object");
  }

  if (lua_gettop(state) == 1) {
    lua_newtable(state);
  } else {
    luaL_checktype(state, 2, LUA_TTABLE);
  }

  LuaWrap::PushVector(state, scene->hierarchy,
                      [](lua_State *state, const SceneObject &obj) -> void {
                        auto error = LuaWrap::PushVariant(state, obj);
                        if (Error::IsError(error)) {
                          luaL_error(state, "Failed to push scene object: %s",
                                     error.message.c_str());
                        }
                      });

  return 1;
}
auto Scene::AddHierarchyObject(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);
  if (scene == nullptr) {
    return luaL_error(state, "Invalid scene object");
  }

  auto result = LuaWrap::VariantFromLua<SceneObject>(state, 2);
  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to parse scene object: %s",
                      result.error().message.c_str());
  }

  scene->hierarchy.emplace_back(result.value());

  return 1;
}
auto Scene::GetHierarchyObject(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);
  if (scene == nullptr) {
    return luaL_error(state, "Invalid scene object");
  }

  if (lua_gettop(state) != 2) {
    return luaL_error(state, "Expected exactly one argument");
  }

  auto index = static_cast<size_t>(luaL_checkinteger(state, 2));
  if (index < 1 || index > scene->hierarchy.size()) {
    return luaL_error(state, "Index out of bounds");
  }

  const auto &hierarchyObject = scene->hierarchy[index - 1];

  auto error = LuaWrap::PushVariant(state, hierarchyObject);
  if (Error::IsError(error)) {
    return luaL_error(state, "Failed to push scene object: %s",
                      error.message.c_str());
  }

  return 1;
}
auto Scene::RemoveHierarchyObject(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);
  if (scene == nullptr) {
    return luaL_error(state, "Invalid scene object");
  }

  if (lua_gettop(state) != 2) {
    return luaL_error(state, "Expected exactly one argument");
  }

  auto index = static_cast<size_t>(luaL_checkinteger(state, 2));
  if (index < 1 || index > scene->hierarchy.size()) {
    return luaL_error(state, "Index out of bounds");
  }

  scene->hierarchy.erase(scene->hierarchy.begin() +
                         static_cast<int>(index - 1));

  return 0;
}
auto Scene::GetHierarchyObjectCount(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);
  if (scene == nullptr) {
    return luaL_error(state, "Invalid scene object");
  }

  lua_pushinteger(state, static_cast<lua_Integer>(scene->hierarchy.size()));
  return 1;
}

auto Scene::DrawUiElement() -> Error {
  ImGui::Text("Scene: %s", name.c_str());
  ImGui::Text("Objects: %zu", hierarchy.size());

  return Error::Success();
}

} // namespace Engine