#pragma once

#include "Modules/error.hpp"
#include "graphics.hpp"
namespace Graphics {
auto Present(GraphicsContext &context) -> Error;
auto InitializeGraphics(GraphicsContext &context) -> Error;
} // namespace Graphics