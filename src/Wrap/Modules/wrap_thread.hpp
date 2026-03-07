#pragma once

#include "Modules/object.hpp"
#include "Modules/thread.hpp"
#include "Wrap/Modules/wrap_channel.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <vector>
namespace Wrap::Threading {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::vector<Ref<::Threading::Thread>> Threads;

auto wrap_NewThread(lua_State *state) -> int;
auto wrap_StartThread(lua_State *state) -> int;
auto wrap_WaitThread(lua_State *state) -> int;
auto wrap_GetThreadStatus(lua_State *state) -> int;
auto wrap_GetThreadErrorMessage(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> ThreadLib = {
    {"start", wrap_StartThread},
    {"wait", wrap_WaitThread},
    {"getStatus", wrap_GetThreadStatus},
    {"getError", wrap_GetThreadErrorMessage},

};

extern "C" inline auto luaopen_thread(lua_State *state) -> int {
  PrintDebug("Registering Thread Lua type.");

  LuaWrap::RegisterLuaType(state, ::Threading::Thread::GetType(),
                           ThreadLib); // NOLINT

  return 1;
}

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> ThreadingLib = {
    {"newThread", wrap_NewThread},
    {"newChannel", Wrap::Threading::wrap_NewChannel},

};

const static std::vector<lua_CFunction> childrenInitFunctions = {
    luaopen_thread,
    Wrap::Threading::luaopen_channel,
};

extern "C" inline auto luaopen_threading(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "thread",
      .Functions = ThreadingLib,                      // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT

  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Threading