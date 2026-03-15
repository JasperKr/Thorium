#pragma once

#include "Wrap/Modules/wrap_bytedata.hpp"
#include "Wrap/Modules/wrap_imagedata.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Data {

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> DataLib = {
    {"newBytedata", Data::wrap_NewBytedata},
    {"newImagedata", Image::wrap_NewImagedata},

};

const static std::vector<lua_CFunction> childrenInitFunctions = {
    Data::luaopen_bytedata,
    Image::luaopen_imagedata,
};

extern "C" inline auto luaopen_data(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "data",
      .Functions = DataLib,                           // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT

  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Data