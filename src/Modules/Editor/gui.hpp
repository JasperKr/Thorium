#pragma once

#include "Modules/console.hpp"
#include "guiState.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

// NOLINTNEXTLINE
constexpr unsigned char data[] = {
// NOLINTNEXTLINE
#embed "guiState.h"
};

// NOLINTNEXTLINE
const std::string dataView{reinterpret_cast<const char *>(data), sizeof(data)};

// constexpr auto data = std::string_view{
//     reinterpret_cast<const char*>(#embed "guiState.h"),
//     #embed "guiState.h".size()
// };

const auto luaStateDefinition =
    "local ffi = require(\"ffi\")\nffi.cdef [[\n" + dataView + "]]";

namespace Gui {
inline auto LoadGUIState(lua_State *state) -> GuiState {
  GuiState guiState{};

  PrintAlways(luaStateDefinition);

  return guiState;
}
} // namespace Gui