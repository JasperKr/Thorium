#pragma once

#include <string>

extern "C" {
#include <lua.h>
}

#include "Modules/error.hpp"

namespace Config {

struct ApplicationConfig {
  std::string Identity = "Thorium"; // Filesystem identity
};

auto Configure(lua_State *state) -> Result<ApplicationConfig>;

} // namespace Config