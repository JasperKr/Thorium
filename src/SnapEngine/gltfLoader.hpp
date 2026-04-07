#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
#include "flecs.h"
#include <string>
#include <unordered_map>
namespace glTF {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern std::vector<std::vector<std::uint8_t>> Buffers;
extern std::unordered_map<std::string, std::vector<std::uint8_t>> URICache;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto LoadGltfModel(Graphics::GraphicsContext &context, const std::string &path,
                   flecs::world *world) -> Error;

} // namespace glTF