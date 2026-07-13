#include "wrap_renderer.hpp"
#include "Editor/editor.hpp"
#include "Scene/scene.hpp"
#include "Wrap/wrap.hpp"
#include "renderer.hpp"

namespace Engine::Renderer {
auto wrap_NewMaterial(lua_State *state) -> int { return 0; }

auto wrap_DrawEverything(lua_State *state) -> int {
  auto *scene = ::LuaWrap::ObjectFromLua<Scene>(state, 1);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a Scene object");
  }

  return 0;
}

auto wrap_Initialize(lua_State *state) -> int {
  auto error =
      RendererInstance.Initialize(*Graphics::GetCurrentGraphicsContext());
  if (Error::IsError(error)) {
    return luaL_error(state, "%s", error.message.c_str());
  }

  return 0;
}

auto wrap_ReloadShaders(lua_State *state) -> int {
  RendererInstance.GetShaderManager().ReloadShaders();
  return 0;
}

auto wrap_PickObject(lua_State *state) -> int {
  auto *camera = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));

  Math::Vec2 mousePos =
      Math::Vec2{luaL_checkscalar(state, 2), luaL_checkscalar(state, 3)};

  auto cameraEntity = camera->entity;
  const auto &cameraObj = cameraEntity.get<Camera>();

  LUA_CK_ERR(Editor::Editor::GetEditorInstance().PickEntity(
      cameraObj, *Graphics::GetCurrentGraphicsContext(), mousePos));

  return 0;
}

} // namespace Engine::Renderer