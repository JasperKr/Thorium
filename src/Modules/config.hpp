#pragma once

#include <cstdint>
#include <string>

extern "C" {
#include <lua.h>
}

#include "Modules/error.hpp"
#include "tl/expected.hpp"

const int DefaultWidth = 800;
const int DefaultHeight = 600;

struct WindowSize {
  int32_t width;
  int32_t height;
};

namespace Config {

struct ApplicationConfig {
  bool Vsync = true;
  std::string Title = "Thorium Engine"; // Window title
  std::string Identity = "Thorium";     // Filesystem identity
  WindowSize Size = {DefaultWidth, DefaultHeight};
};

auto Configure(lua_State *state)
    -> tl::expected<ApplicationConfig, Error::Error>;

} // namespace Config