#pragma once

#include "Modules/error.hpp"
#include "graphics.hpp"
namespace Graphics {
auto Present(GraphicsContext &context) -> Error::Error;
void InitializeGraphics(GraphicsContext &context);
} // namespace Graphics