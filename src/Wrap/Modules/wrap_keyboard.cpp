#include "wrap_keyboard.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/Peripherals/keyboard.hpp"
#include "SDL3/SDL_keyboard.h"
#include <vector>

namespace Wrap::Keyboard {

// key: string...
// Returns: boolean anyDown, boolean key1Down, boolean key2Down, ...
auto Wrap_IsDown(lua_State *state) -> int {
  int keyCount = lua_gettop(state);
  bool anyDown = false;
  std::vector<bool> keyStates;
  keyStates.reserve(static_cast<size_t>(keyCount));

  for (int i = 1; i <= keyCount; ++i) {
    std::string key = luaL_checkstring(state, i);
    bool isDown = ::Keyboard::IsDown(key);
    if (isDown) {
      anyDown = true;
    }
    keyStates.emplace_back(isDown);
  }

  lua_pushboolean(state, anyDown ? 1 : 0);
  for (bool keyState : keyStates) {
    lua_pushboolean(state, keyState ? 1 : 0);
  }
  return 1 + keyCount;
}

// scancode: string...
// Returns: boolean isDown, boolean scancode1Down, boolean scancode2Down, ...
auto Wrap_IsScancodeDown(lua_State *state) -> int {
  int scancodeCount = lua_gettop(state);
  bool anyDown = false;
  std::vector<bool> scancodeStates;
  scancodeStates.reserve(static_cast<size_t>(scancodeCount));

  for (int i = 1; i <= scancodeCount; ++i) {
    std::string scancodeName = luaL_checkstring(state, i);

    bool isDown = ::Keyboard::IsScancodeDown(scancodeName);
    if (isDown) {
      anyDown = true;
    }
    scancodeStates.emplace_back(isDown);
  }

  lua_pushboolean(state, anyDown ? 1 : 0);
  for (bool scancodeState : scancodeStates) {
    lua_pushboolean(state, scancodeState ? 1 : 0);
  }
  return 1 + scancodeCount;
}

auto Wrap_SetEnableTextInput(lua_State *state) -> int {
  bool enable = static_cast<bool>(lua_toboolean(state, 1));
  auto *ctx = Graphics::GetCurrentGraphicsContext();

  if (enable) {
    SDL_StartTextInput(ctx->sdlWindow);
  } else {
    SDL_StopTextInput(ctx->sdlWindow);
  }

  return 0;
}

auto Wrap_IsTextInputEnabled(lua_State *state) -> int {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  bool enabled = SDL_TextInputActive(ctx->sdlWindow);
  lua_pushboolean(state, static_cast<int>(enabled));
  return 1;
}

} // namespace Wrap::Keyboard