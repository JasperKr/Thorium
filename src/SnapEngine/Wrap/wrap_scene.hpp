#pragma once

#include "Scene/boundingBox.hpp"
#include "Scene/geometry.hpp"
#include "Scene/levelOfDetail.hpp"
#include "Scene/model.hpp"
#include "Scene/scene.hpp"
#include "Scene/shape.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Engine {

auto wrap_NewScene(lua_State *state) -> int;
auto wrap_LoadModel(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> SceneLib = {
    {"newScene", wrap_NewScene},
    {"loadModel", wrap_LoadModel},
};

const static std::vector<lua_CFunction> childrenInitFunctions = {
    ::Engine::Scene::LoadBinding,
};

extern "C" inline auto luaopen_scene(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "scene",
      .Functions = SceneLib,                          // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT
  };

  RegisterLuaModule(state, module);

  LuaWrap::RegisterLuaType(state, ::Engine::SceneLuaClass);
  LuaWrap::RegisterLuaType(state, ::Engine::ModelClass);
  LuaWrap::RegisterLuaType(state, ::Engine::ShapeClass);
  LuaWrap::RegisterLuaType(state, ::Engine::LevelOfDetailClass);
  LuaWrap::RegisterLuaType(state, ::Engine::BoundingBoxClass);
  LuaWrap::RegisterLuaType(state, ::Engine::GeometryClass);

  return 1;
}

} // namespace Wrap::Engine