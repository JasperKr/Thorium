#include "event.hpp"
#include "Modules/Peripherals/keyboard.hpp"
#include "Modules/window.hpp"
#include "SDL3/SDL_events.h"
#include <mutex>
#include <queue>

namespace Event {

// NOLINTNEXTLINE
bool MainLoopRunning = true;

// NOLINTNEXTLINE
static std::queue<Event> events;

// Events can be pushed from multiple threads (only in the case of an error event, so this mutex won't be contended basically ever)
// But just to be safe if is here.
// NOLINTNEXTLINE
static std::mutex eventsMutex;

inline auto FromSDLEvent(const SDL_Event &sdlEvent) -> Event {
  Event event;

  switch (sdlEvent.type) {
  case SDL_EVENT_KEY_DOWN: {
    event.Name = "keypressed";
    auto keyCode = Keyboard::KeycodeToString(sdlEvent.key.key);
    auto scanCode = Keyboard::ScancodeToString(sdlEvent.key.scancode);
    event.Values.emplace_back(keyCode);
    event.Values.emplace_back(scanCode);
    event.Values.emplace_back(static_cast<bool>(sdlEvent.key.repeat));
    break;
  }
  case SDL_EVENT_KEY_UP: {
    event.Name = "keyreleased";
    auto keyCode = Keyboard::KeycodeToString(sdlEvent.key.key);
    auto scanCode = Keyboard::ScancodeToString(sdlEvent.key.scancode);
    event.Values.emplace_back(keyCode);
    event.Values.emplace_back(scanCode);
    break;
  }
  case SDL_EVENT_FINGER_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    // lua_pushstring(state, "mousepressed");
    // lua_pushnumber(state, event.button.x);
    // lua_pushnumber(state, event.button.y);
    // lua_pushinteger(state, event.button.button);
    // lua_pushboolean(state, event.type == SDL_EVENT_FINGER_DOWN ? 1 : 0);
    // lua_pushinteger(state, event.button.clicks);
    // returnValCount = 6; // NOLINT
    event.Name = "mousepressed";
    event.Values.emplace_back(sdlEvent.button.x);
    event.Values.emplace_back(sdlEvent.button.y);
    event.Values.emplace_back(static_cast<uint32_t>(sdlEvent.button.button));
    event.Values.emplace_back(sdlEvent.type == SDL_EVENT_FINGER_DOWN);
    event.Values.emplace_back(static_cast<uint32_t>(sdlEvent.button.clicks));
    break;
  }
  case SDL_EVENT_FINGER_UP:
  case SDL_EVENT_MOUSE_BUTTON_UP: {
    // lua_pushstring(state, "mousereleased");
    // lua_pushnumber(state, event.button.x);
    // lua_pushnumber(state, event.button.y);
    // lua_pushinteger(state, event.button.button);
    // lua_pushboolean(state, event.type == SDL_EVENT_FINGER_UP ? 1 : 0);
    // returnValCount = 5; // NOLINT
    event.Name = "mousereleased";
    event.Values.emplace_back(sdlEvent.button.x);
    event.Values.emplace_back(sdlEvent.button.y);
    event.Values.emplace_back(static_cast<uint32_t>(sdlEvent.button.button));
    event.Values.emplace_back(sdlEvent.type == SDL_EVENT_FINGER_UP);
    break;
  }
  case SDL_EVENT_MOUSE_MOTION: {
    // lua_pushstring(state, "mousemoved");
    // lua_pushnumber(state, event.motion.x);
    // lua_pushnumber(state, event.motion.y);
    // lua_pushnumber(state, event.motion.xrel);
    // lua_pushnumber(state, event.motion.yrel);
    // returnValCount = 5; // NOLINT
    event.Name = "mousemoved";
    event.Values.emplace_back(sdlEvent.motion.x);
    event.Values.emplace_back(sdlEvent.motion.y);
    event.Values.emplace_back(sdlEvent.motion.xrel);
    event.Values.emplace_back(sdlEvent.motion.yrel);
    break;
  }
  case SDL_EVENT_MOUSE_WHEEL: {
    // lua_pushstring(state, "wheelmoved");
    // lua_pushnumber(state, event.wheel.x);
    // lua_pushnumber(state, event.wheel.y);
    // returnValCount = 3;
    event.Name = "wheelmoved";
    event.Values.emplace_back(sdlEvent.wheel.x);
    event.Values.emplace_back(sdlEvent.wheel.y);
    break;
  }
  case SDL_EVENT_TEXT_EDITING: {
    // TODO: Is this needed?
    // lua_pushstring(state, "textedited");
  } break;
  case SDL_EVENT_TEXT_INPUT: {
    // lua_pushstring(state, "textinput");
    // lua_pushstring(state, event.text.text);
    // returnValCount = 2;
    event.Name = "textinput";
    event.Values.emplace_back(std::string(sdlEvent.text.text));
    break;
  }
  case SDL_EVENT_TERMINATING:
  case SDL_EVENT_QUIT: {
    // lua_pushstring(state, "quit");
    // returnValCount = 1;
    event.Name = "quit";
    break;
  }
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
  case SDL_EVENT_WINDOW_RESIZED: {
    event.Name = "resize";
    event.Values.emplace_back(sdlEvent.window.data1);
    event.Values.emplace_back(sdlEvent.window.data2);
    Window::GetWindowContext()->swapchainOutOfDate = true;
    break;
  }
  default:
    event.Name = "unknown: {}" + std::to_string(sdlEvent.type);
    break;
  }

  return event;
}

auto Push(const Event &event) -> void {
  std::lock_guard<std::mutex> lock(eventsMutex);
  events.push(event);
}

auto Pull() -> void {
  std::lock_guard<std::mutex> lock(eventsMutex);
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    events.push(FromSDLEvent(event));
  }
}

auto Pop() -> std::optional<Event> {
  std::lock_guard<std::mutex> lock(eventsMutex);
  if (events.empty()) {
    return std::nullopt;
  }

  Event event = events.front();
  events.pop();

  return event;
}

auto Quit() -> void { MainLoopRunning = false; }

} // namespace Event
