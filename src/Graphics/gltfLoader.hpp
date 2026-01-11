#pragma once

#include "Modules/error.hpp"
#include "Modules/model.hpp"
#include "fastgltf/types.hpp"
#include <string>
namespace glTF {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern fastgltf::Parser Parser;

auto LoadGltfModel(Graphics::GraphicsContext &context, const std::string &path,
                   Engine::Scene &scene) -> Error;

} // namespace glTF