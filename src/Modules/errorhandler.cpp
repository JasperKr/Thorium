#include "SDL3/SDL_events.h"
auto ErrorHandlerMainLoop() -> void {
  bool running = true;
  SDL_Event event;

  while (running) {
    // Events

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      case SDL_EVENT_KEY_DOWN:
        running = false;
        break;
      default:
        break;
      }
    }
  }
}