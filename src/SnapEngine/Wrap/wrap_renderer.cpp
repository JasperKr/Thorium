#include "wrap_renderer.hpp"
#include "Editor/editor.hpp"
#include "Scene/scene.hpp"
#include "Wrap/wrap.hpp"
#include "renderer.hpp"

namespace Engine::Renderer {
auto wrap_NewMaterial(lua_State *state) -> int { return 0; }

auto wrap_DrawEverything(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));

  return 0;
}

auto wrap_Initialize(lua_State *state) -> int {
  LUA_CK_ERR(
      RendererInstance.Initialize(*Graphics::GetCurrentGraphicsContext()));

  return 0;
}

auto wrap_ReloadShaders(lua_State *state) -> int {
  RendererInstance.GetShaderManager().ReloadShaders();
  return 0;
}

auto wrap_PickObject(lua_State *state) -> int {
  Math::Vec2 mousePos =
      Math::Vec2{luaL_checkscalar(state, 1), luaL_checkscalar(state, 2)};

  LUA_CK_ERR(Editor::Editor::GetEditorInstance().PickEntity(
      *Graphics::GetCurrentGraphicsContext(), mousePos));

  return 0;
}

auto wrap_SetEditorCamera(lua_State *state) -> int {
  auto camera = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));

  Editor::Editor::GetEditorInstance().EditorCamera = camera->entity;

  return 0;
}

} // namespace Engine::Renderer