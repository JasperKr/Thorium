#pragma once

#include "Wrap/wrap.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Wrap::Filesystem {

auto Wrap_Read(lua_State *state) -> int;
auto Wrap_Append(lua_State *state) -> int;
auto Wrap_Write(lua_State *state) -> int;
auto Wrap_FileExists(lua_State *state) -> int;
auto Wrap_GetFileInfo(lua_State *state) -> int;
auto Wrap_Mount(lua_State *state) -> int;
auto Wrap_Unmount(lua_State *state) -> int;
auto Wrap_GetRealPath(lua_State *state) -> int;
auto Wrap_ListFiles(lua_State *state) -> int;
auto Wrap_CreateDirectory(lua_State *state) -> int;

auto Wrap_GetSaveDirectory(lua_State *state) -> int;
auto Wrap_GetSourceDirectory(lua_State *state) -> int;
auto Wrap_GetSourceBaseDirectory(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg FilesystemLib[] = {
    {"read", Wrap_Read},
    {"append", Wrap_Append},
    {"write", Wrap_Write},
    {"fileExists", Wrap_FileExists},
    {"getfileinfo", Wrap_GetFileInfo},
    {"mount", Wrap_Mount},
    {"unmount", Wrap_Unmount},
    {"getRealPath", Wrap_GetRealPath},
    {"listFiles", Wrap_ListFiles},
    {"createDirectory", Wrap_CreateDirectory},
    {"getSaveDirectory", Wrap_GetSaveDirectory},
    {"getSourceDirectory", Wrap_GetSourceDirectory},
    {"getSourceBaseDirectory", Wrap_GetSourceBaseDirectory},
    {nullptr, nullptr},
};

// nullptr-terminated
const static lua_CFunction *const childrenInitFunctions = {nullptr};

extern "C" inline auto luaopen_filesystem(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "filesystem",
      .Functions = FilesystemLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,
      .ModuleType = nullptr,
  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Filesystem