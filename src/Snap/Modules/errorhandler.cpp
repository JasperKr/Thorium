#include "Modules/console.hpp"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
auto ErrorHandlerMainLoop() -> void {
  bool running = true;
  SDL_Event event;

  // Check if SDL is initialized

  if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    PrintFatal("SDL is not initialized. Exiting error handler loop.");
    return;
  }

  PrintInfo("Entering error handler ...");

  while (running) {
    // Events

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_KEY_UP:
        running = false;
        PrintInfo("Exiting error handler ...");
        break;
      default:
        break;
      }
    }
  }
}