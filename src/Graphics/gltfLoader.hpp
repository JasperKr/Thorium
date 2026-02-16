#pragma once

#include "Modules/error.hpp"
#include "Modules/scene.hpp"
#include <string>
namespace glTF {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
auto LoadGltfModel(Graphics::GraphicsContext &context, const std::string &path,
                   Engine::Scene &scene) -> Error;

} // namespace glTF