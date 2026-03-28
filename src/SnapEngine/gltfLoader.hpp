#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
#include <string>
namespace glTF {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
auto LoadGltfModel(Graphics::GraphicsContext &context, const std::string &path)
    -> Error;

} // namespace glTF