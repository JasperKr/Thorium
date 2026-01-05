#include "wrap_gui.hpp"
#include "Modules/Editor/gui.hpp"
#include <functional>
#include <map>
#include <string>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

namespace Wrap::Gui {
const std::map<std::string,
               std::function<int(lua_State *state, GuiState guiState)>>
    UserInterfaceCallbacks = {};

auto DrawEngineUIComponent(lua_State *state) -> int {
  const auto *componentName = luaL_checkstring(state, 1);
  auto iterator = UserInterfaceCallbacks.find(componentName);
  if (iterator != UserInterfaceCallbacks.end()) {
    GuiState guiState{};
    return iterator->second(state, guiState);
  }

  return luaL_error(state, "Unknown UI component: %s", componentName);
}
} // namespace Wrap::Gui