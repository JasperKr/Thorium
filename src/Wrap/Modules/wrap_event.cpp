#include "wrap_event.hpp"
#include "Modules/Peripherals/keyboard.hpp"
#include "Modules/event.hpp"
#include "SDL3/SDL_events.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#include <string>

namespace Event {
auto wrap_Quit(lua_State *state) -> int {
  MainLoopRunning = false;
  return 0; // Number of return values
}

auto wrap_Pull(lua_State *state) -> int {
  Event::Pull();

  return 0; // Number of return values
}

auto wrap_Pop(lua_State *state) -> int {
  SDL_Event *event = Event::Pop();

  if (event == nullptr) {
    return 0;
  }

  int32_t returnValCount = 0;

  switch (event->type) {
  case SDL_EVENT_KEY_DOWN: {
    lua_pushstring(state, "keypressed");

    auto keycodeIter =
        Keyboard::KeycodeToString.find(static_cast<uint32_t>(event->key.key));
    std::string keycode = "unknown";
    if (keycodeIter != Keyboard::KeycodeToString.end()) {
      keycode = keycodeIter->second;
    }

    auto scancodeIter = Keyboard::ScancodeToString.find(
        static_cast<uint32_t>(event->key.scancode));
    std::string scancode = "unknown";
    if (scancodeIter != Keyboard::ScancodeToString.end()) {
      scancode = scancodeIter->second;
    }

    lua_pushstring(state, keycode.c_str());
    lua_pushstring(state, scancode.c_str());
    lua_pushboolean(state, static_cast<int>(event->key.repeat));
    returnValCount = 4;
    break;
  }
  case SDL_EVENT_KEY_UP: {
    lua_pushstring(state, "keyreleased");

    auto keycodeIter =
        Keyboard::KeycodeToString.find(static_cast<uint32_t>(event->key.key));
    std::string keycode = "unknown";
    if (keycodeIter != Keyboard::KeycodeToString.end()) {
      keycode = keycodeIter->second;
    }

    auto scancodeIter = Keyboard::ScancodeToString.find(
        static_cast<uint32_t>(event->key.scancode));
    std::string scancode = "unknown";
    if (scancodeIter != Keyboard::ScancodeToString.end()) {
      scancode = scancodeIter->second;
    }

    lua_pushstring(state, keycode.c_str());
    lua_pushstring(state, scancode.c_str());

    returnValCount = 3;
    break;
  }
  case SDL_EVENT_FINGER_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    lua_pushstring(state, "mousepressed");
    lua_pushnumber(state, event->button.x);
    lua_pushnumber(state, event->button.y);
    lua_pushinteger(state, event->button.button);
    lua_pushboolean(state, event->type == SDL_EVENT_FINGER_DOWN ? 1 : 0);
    lua_pushinteger(state, event->button.clicks);
    returnValCount = 6; // NOLINT
    break;
  }
  case SDL_EVENT_FINGER_UP:
  case SDL_EVENT_MOUSE_BUTTON_UP: {
    lua_pushstring(state, "mousereleased");
    lua_pushnumber(state, event->button.x);
    lua_pushnumber(state, event->button.y);
    lua_pushinteger(state, event->button.button);
    lua_pushboolean(state, event->type == SDL_EVENT_FINGER_UP ? 1 : 0);
    returnValCount = 5; // NOLINT
    break;
  }
  case SDL_EVENT_MOUSE_MOTION: {
    lua_pushstring(state, "mousemoved");
    lua_pushnumber(state, event->motion.x);
    lua_pushnumber(state, event->motion.y);
    lua_pushnumber(state, event->motion.xrel);
    lua_pushnumber(state, event->motion.yrel);
    returnValCount = 5; // NOLINT
    break;
  }
  case SDL_EVENT_MOUSE_WHEEL: {
    lua_pushstring(state, "wheelmoved");
    lua_pushnumber(state, event->wheel.x);
    lua_pushnumber(state, event->wheel.y);
    returnValCount = 3;
    break;
  }
  case SDL_EVENT_TEXT_EDITING: {
    // TODO: Is this needed?
    // lua_pushstring(state, "textedited");
  } break;
  case SDL_EVENT_TEXT_INPUT: {
    lua_pushstring(state, "textinput");
    lua_pushstring(state, event->text.text);
    returnValCount = 2;
    break;
  }
  case SDL_EVENT_TERMINATING:
  case SDL_EVENT_QUIT: {
    lua_pushstring(state, "quit");
    returnValCount = 1;
    break;
  }
  default:
    break;
  }

  return returnValCount; // Number of return values
}

} // namespace Event