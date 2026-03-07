#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <vector>

static const std::vector<luaL_Reg> RendererLib = {

};

static const std::vector<lua_CFunction> childrenInitFunctions{};

extern "C" inline auto luaopen_renderer(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "renderer",
      .Functions = RendererLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,
  };

  RegisterLuaModule(state, module);
  return 1;
}