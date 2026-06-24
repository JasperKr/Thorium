#pragma once

#include <forward_list>
#include <optional>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>
namespace Graphics {

inline auto GetIsCurrentlyRendering() -> bool & {
  thread_local bool CurrentlyRendering = false;
  return CurrentlyRendering;
}

inline auto GetIsStateDirty() -> bool & {
  thread_local bool StateDirty = true;
  return StateDirty;
}

// Use if the state gets invalidated,
// for example, new command buffer.
inline auto SetDirtyState() -> void { GetIsStateDirty() = true; }

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline auto DefaultPixelFormat = VK_FORMAT_R8G8B8A8_UNORM;
constexpr uint32_t MAX_COLOR_ATTACHMENTS = 8;

constexpr VkPipelineColorBlendAttachmentState DefaultBlendMode = {
    .blendEnable = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_G_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_B_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_A_BIT),
};

constexpr VkPipelineColorBlendAttachmentState BlendmodeNone = {
    .blendEnable = VK_FALSE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_G_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_B_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_A_BIT),
};

constexpr uint32_t FRAMES_IN_FLIGHT = 3;
constexpr uint32_t MAX_SWAPCHAIN_IMAGES = 8;

struct KeyElement {
  KeyElement() = default;
  KeyElement(const char *key) : Key(key) {} // NOLINT
  explicit KeyElement(uint64_t index) : Index(index), IsIndex(true) {}
  explicit KeyElement(std::string_view key) : Key(key.data()) {}

  union {
    const char *Key;
    uint64_t Index{};
  };

  bool IsIndex = false;

  [[nodiscard]] auto Matches(std::string_view key) const -> bool {
    return !IsIndex && Key == key;
  }

  [[nodiscard]] auto Matches(const char *key) const -> bool {
    return Matches(std::string_view{key});
  }

  [[nodiscard]] auto GetIndex() const -> std::optional<uint64_t> {
    if (!IsIndex) {
      return std::nullopt;
    }

    return Index;
  }

  [[nodiscard]] auto ToString() const -> std::string {
    if (IsIndex) {
      return std::to_string(Index);
    }
    return Key;
  }
};

using ResourceKey = std::forward_list<KeyElement>;

} // namespace Graphics