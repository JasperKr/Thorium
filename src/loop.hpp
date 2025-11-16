#pragma once

#include "Modules/error.hpp"
#include <string>

const int DefaultWidth = 800;
const int DefaultHeight = 600;

struct WindowSize {
  int32_t width;
  int32_t height;
};

struct ApplicationConfig {
  bool Vsync = true;
  std::string Title = "Thorium Engine"; // Window title
  std::string Identity = "Thorium";     // Filesystem identity
  WindowSize Size = {DefaultWidth, DefaultHeight};
};

auto MainLoop() -> Error::Error;