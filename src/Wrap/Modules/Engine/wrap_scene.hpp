#pragma once

#include "Modules/Engine/scene.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Engine {

auto wrap_NewScene(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg SceneLib[] = {
    {"newScene", wrap_NewScene},
    {nullptr, nullptr},
};

// nullptr-terminated NOLINTNEXTLINE
const static lua_CFunction childrenInitFunctions[] = {
    ::Engine::Scene::LoadBinding,
    nullptr,
};

extern "C" inline auto luaopen_scene(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "scene",
      .Functions = SceneLib,                          // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT
      .ModuleType = nullptr,
  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Engine