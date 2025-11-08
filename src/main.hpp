#pragma once

#include <string>

struct ApplicationConfig {
  bool Vsync = true;
  std::string Title = "Thorium Engine"; // Window title
  std::string Identity = "Thorium";     // Filesystem identity
};