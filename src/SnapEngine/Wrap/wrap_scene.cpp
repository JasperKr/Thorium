#include "wrap_scene.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/object.hpp"
#include "Scene/scene.hpp"
#include "Wrap/wrap.hpp"
#include "gltfLoader.hpp"

namespace Wrap::Engine {

auto wrap_NewScene(lua_State *state) -> int {
  const char *name = luaL_checkstring(state, 1);

  auto scene = Ref<::Engine::Scene>::Make(name);

  LuaWrap::PushObject(state, ::Engine::Scene::GetType(), scene.get());
  return 1;
}

auto wrap_LoadModel(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Engine::Scene>(state, 1));

  const char *path = luaL_checkstring(state, 2);

  auto *ctx = Graphics::GetCurrentGraphicsContext();

  auto loadResult = glTF::LoadGltfModel(*ctx, path, &scene->world);
  if (Error::IsError(loadResult)) {
    luaL_error(state, loadResult.message.c_str());
  }

  return 0;
}

} // namespace Wrap::Engine