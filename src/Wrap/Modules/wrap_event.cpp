#include "wrap_event.hpp"
#include "Modules/Peripherals/keyboard.hpp"
#include "Modules/console.hpp"
#include "Modules/event.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#include <string>

namespace Wrap::Event {
auto wrap_Quit(lua_State *state) -> int {
  ::Event::MainLoopRunning = false;

  PrintAlways("Quit event received, stopping main loop.");

  return 0; // Number of return values
}

auto wrap_Pull(lua_State *state) -> int {
  ::Event::Pull();

  return 0; // Number of return values
}

auto wrap_Pop(lua_State *state) -> int {
  auto eventResult = ::Event::Pop();

  if (!eventResult.has_value()) {
    return 0;
  }

  ::Event::Event &event = eventResult.value();

  lua_pushstring(state, event.Name.c_str());

  for (auto &value : event.Values) {
    if (auto *intValue = std::get_if<int32_t>(&value)) {
      lua_pushinteger(state, *intValue);
    } else if (auto *uintValue = std::get_if<uint32_t>(&value)) {
      lua_pushinteger(state, static_cast<lua_Integer>(*uintValue));
    } else if (auto *floatValue = std::get_if<float>(&value)) {
      lua_pushnumber(state, static_cast<lua_Number>(*floatValue));
    } else if (auto *doubleValue = std::get_if<double>(&value)) {
      lua_pushnumber(state, static_cast<lua_Number>(*doubleValue));
    } else if (auto *stringValue = std::get_if<std::string>(&value)) {
      lua_pushstring(state, stringValue->c_str());
    } else if (auto *boolValue = std::get_if<bool>(&value)) {
      lua_pushboolean(state, *boolValue ? 1 : 0);
    } else {
      lua_pushnil(state); // Unknown type
    }
  }

  return static_cast<int>(event.Values.size() + 1); // Number of return values
}

} // namespace Wrap::Event