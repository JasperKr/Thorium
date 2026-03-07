#include "Modules/timer.hpp"
#include "lua.hpp"

namespace Wrap::Timer {
auto wrap_GetTime(lua_State *state) -> int {
  auto time = ::Timer::GetTime();

  lua_pushnumber(state, time);

  return 1;
}

auto wrap_GetDelta(lua_State *state) -> int {
  auto delta = ::Timer::GetDelta();

  lua_pushnumber(state, delta);

  return 1;
}

auto wrap_GetAverageDelta(lua_State *state) -> int {
  auto averageDelta = ::Timer::GetAverageDelta();

  lua_pushnumber(state, averageDelta);

  return 1;
}

auto wrap_GetFPS(lua_State *state) -> int {
  auto fps = ::Timer::GetFPS();

  lua_pushnumber(state, fps);

  return 1;
}

auto wrap_Sleep(lua_State *state) -> int {
  double seconds = luaL_checknumber(state, 1);

  ::Timer::Sleep(seconds, true);

  return 0;
}

auto wrap_Step(lua_State *state) -> int {
  ::Timer::Step();

  return 0;
}

} // namespace Wrap::Timer