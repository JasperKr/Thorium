#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "SDL3/SDL_mouse.h"
#include <map>
#include <string>
namespace Mouse {

const std::map<std::string, int> MouseButtonMap = {
    {"left", SDL_BUTTON_LEFT},   {"middle", SDL_BUTTON_MIDDLE},
    {"right", SDL_BUTTON_RIGHT}, {"x1", SDL_BUTTON_X1},
    {"x2", SDL_BUTTON_X2},
};

const static Type type = Type("Mouse Cursor");

struct MouseCursor : Object {
  MouseCursor(const MouseCursor &) = delete;
  MouseCursor(MouseCursor &&) = delete;
  auto operator=(const MouseCursor &) -> MouseCursor & = delete;
  auto operator=(MouseCursor &&) -> MouseCursor & = delete;
  explicit MouseCursor(SDL_Cursor *sdlCursor) : sdlCursor(sdlCursor) {}
  SDL_Cursor *sdlCursor;

  ~MouseCursor() override {
    if (sdlCursor != nullptr) {
      SDL_DestroyCursor(sdlCursor);
      sdlCursor = nullptr;
    }
  }

  [[nodiscard]] static auto GetType() -> Type const * { return &type; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return MouseCursor::GetType();
  }
};

} // namespace Mouse