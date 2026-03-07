#include "wrap_snap.hpp"
#include "Modules/Engine/scene.hpp"
#include "Modules/object.hpp"

namespace Wrap::Engine {

auto wrap_NewScene(lua_State *state) -> int {
  auto scene = Ref<::Engine::Scene>::Make();
  LuaWrap::PushObject(state, ::Engine::Scene::GetType(), scene.get());
  return 1;
}

} // namespace Wrap::Engine