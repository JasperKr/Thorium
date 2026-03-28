#pragma once

#include "Scene/boundingBox.hpp"
#include "Scene/levelOfDetail.hpp"
#include "Scene/model.hpp"
#include "Scene/scene.hpp"
#include "Scene/shape.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Engine {

auto wrap_NewScene(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> SceneLib = {
    {"newScene", wrap_NewScene},
};

const static std::vector<lua_CFunction> childrenInitFunctions = {
    ::Engine::Scene::LoadBinding,
    ::Engine::luaopen_Scene,
};

extern "C" inline auto luaopen_scene(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "scene",
      .Functions = SceneLib,                          // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT
      .Children =
          {
              ::Engine::BoundingBoxModule,
              ::Engine::ShapeModule,
              ::Engine::ModelModule,
              ::Engine::LevelOfDetailModule,
          }, // NOLINT
  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Engine