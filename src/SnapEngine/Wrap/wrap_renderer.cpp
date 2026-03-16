#include "wrap_renderer.hpp"
#include "Modules/object.hpp"
#include "material.hpp"

namespace Engine::Renderer {
auto wrap_NewMaterial(lua_State *state) -> int {
  auto material = Ref<Engine::Renderer::Material>::Make();
  LuaWrap::PushObject(state, Engine::Renderer::Material::GetType(),
                      material.get());
  return 1;
}
} // namespace Engine::Renderer