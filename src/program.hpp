#pragma once

#include "Graphics/graphics.hpp"
#include "main.hpp"

namespace Program {

auto Update(double deltaTime) -> Error::Error;
auto Draw(Graphics::GraphicsContext &context) -> Error::Error;
auto Load(Graphics::GraphicsContext &context) -> Error::Error;
auto Exit(Graphics::GraphicsContext &context) -> Error::Error;
auto Configuration(ApplicationConfig &config) -> Error::Error;

} // namespace Program