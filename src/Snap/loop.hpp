#pragma once

#include "Modules/error.hpp"
#include <string>
#include <vector>

auto MainLoop(const std::vector<std::string> &arguments) -> Error;