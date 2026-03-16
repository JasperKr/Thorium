#pragma once

#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include "scene.hpp"

namespace Wrap::Engine {

auto wrap_NewScene(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> SceneLib = {
    {"newScene", wrap_NewScene},

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
  return 1;
}

} // namespace Wrap::Engine