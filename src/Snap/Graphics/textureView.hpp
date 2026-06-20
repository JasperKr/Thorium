#pragma once

#include "Graphics/barrier.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
namespace Graphics {

const static Type LuaTextureViewType = Type("TextureView");

struct TextureViewCreateInfo {
  Ref<Texture> texture;
  VkImageSubresourceRange subresourceRange;
};

struct TextureView : Object, Barrier::BarrierSynced {
  TextureView(const TextureView &) = delete;
  TextureView(TextureView &&) = delete;
  auto operator=(const TextureView &) -> TextureView & = delete;
  auto operator=(TextureView &&) -> TextureView & = delete;

  Ref<Texture> texture;
  VkImageView imageView{};
  VkImageSubresourceRange subresourceRange{};
  uint64_t lastUsedTimestamp{};

  static auto GetType() -> Type const * { return &LuaTextureViewType; }
  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return GetType();
  }

  auto GetTimestamp() const -> uint64_t { return lastUsedTimestamp; }
  auto MarkUse() -> void;

  static auto Create(const GraphicsContext &context,
                     const TextureViewCreateInfo &info)
      -> Result<Ref<TextureView>>;

  ~TextureView() override;

private:
  TextureView() = default;
};

} // namespace Graphics