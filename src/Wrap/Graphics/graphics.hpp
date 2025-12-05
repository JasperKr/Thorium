#pragma once

#include "Wrap/Graphics/rendertarget.hpp"
#include "Wrap/Graphics/texture.hpp"
#include "Wrap/wrap.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
namespace Graphics {

auto wrap_Present(lua_State *state) -> int;

// RenderTarget functions
auto wrap_Push(lua_State *state) -> int;
auto wrap_Pop(lua_State *state) -> int;
auto wrap_Reset(lua_State *state) -> int;

// Rendertarget state functions
auto wrap_SetDepthMode(lua_State *state) -> int;
auto wrap_SetCullMode(lua_State *state) -> int;
auto wrap_SetPolygonMode(lua_State *state) -> int;
auto wrap_SetViewport(lua_State *state) -> int;
auto wrap_SetScissor(lua_State *state) -> int;
auto wrap_ClipScissor(lua_State *state) -> int;
auto wrap_SetShader(lua_State *state) -> int;
auto wrap_SetLineWidth(lua_State *state) -> int;
auto wrap_SetWindingOrder(lua_State *state) -> int;

auto wrap_GetDepthMode(lua_State *state) -> int;
auto wrap_GetCullMode(lua_State *state) -> int;
auto wrap_GetPolygonMode(lua_State *state) -> int;
auto wrap_GetViewport(lua_State *state) -> int;
auto wrap_GetScissor(lua_State *state) -> int;
auto wrap_GetShader(lua_State *state) -> int;
auto wrap_GetRenderTargets(lua_State *state) -> int;
auto wrap_GetLineWidth(lua_State *state) -> int;
auto wrap_GetWindingOrder(lua_State *state) -> int;
auto wrap_Draw(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg GraphicsLib[] = {
    {"present", wrap_Present},
    {"push", wrap_Push},
    {"pop", wrap_Pop},
    {"reset", wrap_Reset},
    {"setDepthMode", wrap_SetDepthMode},
    {"setCullMode", wrap_SetCullMode},
    {"setPolygonMode", wrap_SetPolygonMode},
    {"setViewport", wrap_SetViewport},
    {"setScissor", wrap_SetScissor},
    {"clipScissor", wrap_ClipScissor},
    {"setShader", wrap_SetShader},
    {"setRenderTarget", RenderTarget::wrap_SetRenderTargets},
    {"setLineWidth", wrap_SetLineWidth},
    {"setWindingOrder", wrap_SetWindingOrder},
    {"getDepthMode", wrap_GetDepthMode},
    {"getCullMode", wrap_GetCullMode},
    {"getPolygonMode", wrap_GetPolygonMode},
    {"getViewport", wrap_GetViewport},
    {"getScissor", wrap_GetScissor},
    {"getShader", wrap_GetShader},
    {"getRenderTargets", wrap_GetRenderTargets},
    {"getLineWidth", wrap_GetLineWidth},
    {"getWindingOrder", wrap_GetWindingOrder},
    {"newTexture", Texture::wrap_NewTexture},
    {"draw", wrap_Draw},
    {nullptr, nullptr},
};

// nullptr-terminated NOLINTNEXTLINE
const static lua_CFunction childrenInitFunctions[] = {Texture::luaopen_texture,
                                                      nullptr};

extern "C" inline auto luaopen_graphics(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "graphics",
      .Functions = GraphicsLib,                       // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT
      .ModuleType = nullptr,
  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Graphics