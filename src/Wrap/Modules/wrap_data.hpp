#pragma once

#include "Wrap/Modules/wrap_bytedata.hpp"
#include "Wrap/Modules/wrap_imagedata.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Data {

// NOLINTNEXTLINE
static const luaL_Reg DataLib[] = {
    {"newBytedata", Data::wrap_NewBytedata},
    {"newImagedata", Image::wrap_NewImagedata},
    {nullptr, nullptr},
};

// nullptr-terminated NOLINTNEXTLINE
const static lua_CFunction childrenInitFunctions[] = {
    Data::luaopen_bytedata,
    Image::luaopen_imagedata,
    nullptr,
};

extern "C" inline auto luaopen_data(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "data",
      .Functions = DataLib,                           // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT
      .ModuleType = nullptr,
  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Data