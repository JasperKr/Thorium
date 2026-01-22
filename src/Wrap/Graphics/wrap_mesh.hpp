#pragma once

#include "Graphics/mesh.hpp"
#include "Modules/console.hpp"
#include "Wrap/wrap.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
namespace Graphics {

auto wrap_SetVertices(lua_State *state) -> int;
auto wrap_SetIndices(lua_State *state) -> int;

auto wrap_SetVertexBuffer(lua_State *state) -> int;
auto wrap_SetIndexBuffer(lua_State *state) -> int;

auto wrap_SetDrawRange(lua_State *state) -> int;
auto wrap_GetDrawRange(lua_State *state) -> int;

auto wrap_GetVertexCount(lua_State *state) -> int;
auto wrap_GetIndexCount(lua_State *state) -> int;

auto wrap_NewMesh(lua_State *state) -> int;
auto wrap_Release(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg MeshLib[] = {
    {"setVertices", wrap_SetVertices},
    {"setIndices", wrap_SetIndices},
    {"setVertexBuffer", wrap_SetVertexBuffer},
    {"setIndexBuffer", wrap_SetIndexBuffer},
    {"getVertexCount", wrap_GetVertexCount},
    {"getIndexCount", wrap_GetIndexCount},
    {"setDrawRange", wrap_SetDrawRange},
    {"getDrawRange", wrap_GetDrawRange},
    {"release", wrap_Release},
    {nullptr, nullptr} // terminate with nullptr
};

extern "C" inline auto luaopen_mesh(lua_State *state) -> int {
  PrintDebug("Registering Mesh Lua type.");

  LuaWrap::RegisterLuaType(state, Mesh::GetType(),
                           MeshLib); // NOLINT

  return 1;
}

} // namespace Graphics