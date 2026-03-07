#pragma once

#include "Modules/Engine/scene.hpp"
#include "Wrap/wrap.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Wrap::Engine {

auto wrap_NewScene(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg SnapLib[] = {
    {nullptr, nullptr},
};

// nullptr-terminated NOLINTNEXTLINE
const static lua_CFunction childrenInitFunctions[] = {
    ::Engine::Scene::LoadBinding,
    nullptr,
};

extern "C" inline auto luaopen_snap(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "snap",
      .Functions = SnapLib,                           // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT
      .ModuleType = nullptr,
  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Engine