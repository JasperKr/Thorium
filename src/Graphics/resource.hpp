#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/texture.hpp"
#include "Modules/object.hpp"
#include <cassert>
#include <mutex>

namespace Graphics {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern std::vector<Ref<Texture::Texture>> ReleasedTextures;
extern std::vector<Ref<Buffer>> ReleasedBuffers;

extern std::mutex ReleasedTexturesMutex;
extern std::mutex ReleasedBuffersMutex;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto ProcessReleasedResources(GraphicsContext &context) -> void;

auto ScheduleDestruction(Texture::Texture *texture) -> void;
auto ScheduleDestruction(Buffer *buffer) -> void;

} // namespace Graphics