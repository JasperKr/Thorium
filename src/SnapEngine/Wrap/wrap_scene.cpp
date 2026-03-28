#include "wrap_scene.hpp"
#include "Modules/object.hpp"
#include "Scene/scene.hpp"
#include "Wrap/wrap.hpp"

namespace Wrap::Engine {

auto wrap_NewScene(lua_State *state) -> int {
  const char *name = luaL_checkstring(state, 1);

  auto scene = Ref<::Engine::Scene>::Make(name);

  LuaWrap::PushObject(state, ::Engine::Scene::GetType(), scene.get());
  return 1;
}

} // namespace Wrap::Engine