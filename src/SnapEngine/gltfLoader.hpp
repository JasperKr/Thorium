#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
#include "Scene/scene.hpp"
#include <string>
namespace glTF {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
auto LoadGltfModel(Graphics::GraphicsContext &context, const std::string &path,
                   Engine::Scene &scene) -> Error;

} // namespace glTF