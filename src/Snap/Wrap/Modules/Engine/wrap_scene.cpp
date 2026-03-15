#include "wrap_scene.hpp"
#include "Modules/Engine/scene.hpp"
#include "Modules/object.hpp"
#include "Wrap/wrap.hpp"

namespace Wrap::Engine {

auto wrap_NewScene(lua_State *state) -> int {
  auto name = std::string(luaL_optstring(state, 1, "New Scene"));

  auto scene = Ref<::Engine::Scene>::Make(name);
  LuaWrap::PushObject(state, ::Engine::Scene::GetType(), scene.get());
  return 1;
}

} // namespace Wrap::Engine