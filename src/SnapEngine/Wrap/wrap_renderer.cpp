#include "wrap_renderer.hpp"
#include "Scene/scene.hpp"
#include "Wrap/wrap.hpp"

namespace Engine::Renderer {
auto wrap_NewMaterial(lua_State *state) -> int { return 0; }
auto wrap_DrawEverything(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a Scene object");
  }

  return 0;
}
} // namespace Engine::Renderer