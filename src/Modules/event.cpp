#include "event.hpp"
#include "SDL3/SDL_events.h"
#include <queue>

namespace Event {

// NOLINTNEXTLINE
bool MainLoopRunning = true;

// NOLINTNEXTLINE
static std::queue<SDL_Event> events;

auto OnKeyEvent(const SDL_KeyboardEvent &keyEvent) -> void {}
auto OnMouseEvent(const SDL_MouseButtonEvent &mouseEvent) -> void {}
auto OnMouseMotionEvent(const SDL_MouseMotionEvent &mouseMotionEvent) -> void {}
auto OnMouseWheelEvent(const SDL_MouseWheelEvent &mouseWheelEvent) -> void {}
auto OnTextEditEvent(const SDL_TextEditingEvent &textEditEvent) -> void {}
auto OnTextInputEvent(const SDL_TextInputEvent &textInputEvent) -> void {}

auto Pull() -> void {
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    events.push(event);

    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      OnKeyEvent(event.key);
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      OnMouseEvent(event.button);
      break;
    case SDL_EVENT_MOUSE_MOTION:
      OnMouseMotionEvent(event.motion);
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      OnMouseWheelEvent(event.wheel);
      break;
    case SDL_EVENT_TEXT_EDITING:
      OnTextEditEvent(event.edit);
      break;
    case SDL_EVENT_TEXT_INPUT:
      OnTextInputEvent(event.text);
      break;
    default:
      break;
    }
  }
}

auto Pop() -> std::optional<SDL_Event> {
  if (events.empty()) {
    return std::nullopt;
  }

  SDL_Event event = events.front();
  events.pop();

  return event;
}

auto Quit() -> void { MainLoopRunning = false; }

} // namespace Event
