#include "wrap_event.hpp"
#include "Modules/Peripherals/keyboard.hpp"
#include "Modules/console.hpp"
#include "Modules/event.hpp"
#include "Wrap/wrap.hpp"

#include <lua.h>
#include <lua.hpp>
#include <string>

namespace Wrap::Event {
auto wrap_Quit(lua_State *state) -> int {
  ::Event::Push(::Event::Event{
      .Name = "quit",
      .Values = {},
  });

  PrintInfo("Quit event received, stopping main loop.");

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

  // take table from stack and fill it with event data or create a new table if none exists
  if (lua_isnoneornil(state, 1)) {
    lua_newtable(state);
  }

  LUA_ASSERT(lua_gettop(state) == 1);

  ::Event::Event &event = eventResult.value();
  int index = 1;

  lua_pushstring(state, event.Name.c_str());
  lua_rawseti(state, 1, index++);

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
    lua_rawseti(state, 1, index++);
  }

  lua_pushinteger(state, index - 1);

  assert(lua_gettop(state) == 2);

  return 2; // Number of return values (the table, table size + name)
}

} // namespace Wrap::Event