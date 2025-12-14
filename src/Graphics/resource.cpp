#include "resource.hpp"

namespace Graphics {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<Ref<Texture::Texture>> ReleasedTextures{};
std::vector<Ref<Buffer>> ReleasedBuffers{};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace Graphics