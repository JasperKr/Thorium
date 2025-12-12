#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/resource.hpp"
#include "Graphics/texture.hpp"
#include <vector>

namespace Graphics {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
inline std::vector<Graphics::ReleasedResource> ReleasedResources;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

inline auto AddReleasedResource(ReleasedResource resource) -> void {
  ReleasedResources.push_back(resource);
}

inline auto ProcessReleasedResources(GraphicsContext &context,
                                     uint64_t completedTimelineValue) -> void {
  if (ReleasedResources.empty()) {
    return;
  }
  for (auto i = ReleasedResources.size() - 1; i >= 0; i--) {
    ReleasedResource res = ReleasedResources.at(i);
    switch (res.type) {
    case ReleasedResourceType::BUFFER: {
      auto *buffer = std::get<Buffer *>(res.resource);
      if (buffer->lastUsedTimelineValue <= completedTimelineValue) {
        buffer->Destroy(context);
        ReleasedResources.erase(ReleasedResources.begin() +
                                static_cast<int64_t>(i));
      }
      break;
    }
    case ReleasedResourceType::TEXTURE: {
      auto *texture = std::get<Texture::Texture *>(res.resource);
      if (texture->lastUsedTimelineValue <= completedTimelineValue) {
        texture->Destroy(context);
        ReleasedResources.erase(ReleasedResources.begin() +
                                static_cast<int64_t>(i));
      }
      break;
    }
    default:
      break;
    }
  }
}
} // namespace Graphics