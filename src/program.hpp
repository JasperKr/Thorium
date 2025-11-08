#pragma once

#include "Graphics/graphics.hpp"
#include "main.hpp"

namespace Program {

void Update(double deltaTime);
void Draw(Graphics::GraphicsContext &context);
void Load(Graphics::GraphicsContext &context);
void Exit(Graphics::GraphicsContext &context);
void Configuration(ApplicationConfig &config);

} // namespace Program