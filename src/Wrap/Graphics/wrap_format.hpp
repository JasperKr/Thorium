#pragma once

#include "Graphics/bufferformat.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Wrap::Graphics {

auto FormatFromLua(lua_State *state, int index,
                   ::Graphics::Standard standard = ::Graphics::Standard::Std430)
    -> Result<::Graphics::BufferFormat>;
auto SimpleFormatFromLua(
    lua_State *state, int index,
    ::Graphics::Standard standard = ::Graphics::Standard::Std430)
    -> Result<::Graphics::BufferFormat>;

} // namespace Wrap::Graphics
