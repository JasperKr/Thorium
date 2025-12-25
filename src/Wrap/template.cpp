extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#include <vector>

namespace WrapTemplate {
auto wrap_Function(lua_State *state) -> int {
  // stuff...

  return 0; // Number of return values
}

const std::vector<luaL_Reg> TemplateLib = {
    {"Function", wrap_Function},
};

} // namespace WrapTemplate