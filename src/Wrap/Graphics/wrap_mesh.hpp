#pragma once

#include "Graphics/mesh.hpp"
#include "Modules/console.hpp"
#include "Wrap/wrap.hpp"
#include <lauxlib.h>
#include <lua.h>
namespace Graphics {

auto Wrap_SetVertices(lua_State *state) -> int;
auto Wrap_SetIndices(lua_State *state) -> int;

auto Wrap_SetVertexBuffer(lua_State *state) -> int;
auto Wrap_SetIndexBuffer(lua_State *state) -> int;

auto Wrap_SetDrawRange(lua_State *state) -> int;
auto Wrap_GetDrawRange(lua_State *state) -> int;

auto Wrap_NewMesh(lua_State *state) -> int;
auto Wrap_Release(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg MeshLib[] = {
    {"release", Wrap_Release}, {nullptr, nullptr} // terminate with nullptr
};

extern "C" inline auto luaopen_mesh(lua_State *state) -> int {
  PrintDebug("Registering Mesh Lua type.");

  LuaWrap::RegisterLuaType(state, Mesh::GetType(),
                           MeshLib); // NOLINT

  return 1;
}

} // namespace Graphics