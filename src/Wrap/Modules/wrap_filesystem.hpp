#pragma once

#include "Wrap/wrap.hpp"
#include "lua.hpp"

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
static const std::vector<luaL_Reg> FilesystemLib = {
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

};

const static std::vector<lua_CFunction> childrenInitFunctions{};

extern "C" inline auto luaopen_filesystem(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "filesystem",
      .Functions = FilesystemLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,

  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Filesystem