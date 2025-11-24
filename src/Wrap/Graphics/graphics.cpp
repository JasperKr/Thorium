#include "Graphics/graphics.hpp"
#include "Graphics/render.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Graphics {
auto wrap_Present(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto result = Present(*ctx);

  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.ToString().c_str());
  }

  return 0;
}
} // namespace Graphics