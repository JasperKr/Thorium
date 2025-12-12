#pragma once

#include <concepts>
#include <cstdint>
#include <variant>

template <typename T>
concept GraphicsResource = requires(T resource) {
  { resource.lastUsedTimelineValue } -> std::convertible_to<uint64_t &>;
};

namespace Graphics {
struct Buffer;
namespace Texture {
struct Texture;
}

enum class ReleasedResourceType : uint8_t { BUFFER, TEXTURE };
struct ReleasedResource {
  ReleasedResourceType type;
  std::variant<Buffer *, Texture::Texture *> resource;
};

} // namespace Graphics