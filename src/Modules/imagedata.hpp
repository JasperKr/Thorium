#pragma once

#include "../Graphics/format.hpp"
#include "Math/vector.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/error.hpp"
#include "Modules/type.hpp"
#include "color.hpp"
#include "tl/expected.hpp"
#include <cassert>
#include <cstddef>
#include <span>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include <cstdint>
namespace Image {

static const Type type = Type("ImageData");

struct ImageData : Data::ByteData {
public:
  auto SetColor(Math::Uvec2 position, const Color &color) -> void;
  auto GetColor(Math::Uvec2 position) -> Color &;
  auto Copy(const ImageData &source) -> void;
  auto GetDataPtr() -> uint8_t * { return Data::ByteData::GetData(); }
  [[nodiscard]] auto GetSize() const -> size_t {
    return Data::ByteData::GetSize();
  }
  [[nodiscard]] auto GetWidth() const -> uint32_t { return width; }
  [[nodiscard]] auto GetHeight() const -> uint32_t { return height; }
  [[nodiscard]] auto GetFormat() const -> VkFormat { return format; }
  [[nodiscard]] auto GetChannelCount() const -> uint32_t {
    return Graphics::Format::GetChannelCount(format);
  }
  [[nodiscard]] auto GetFormatSize() const -> size_t {
    return Graphics::Format::GetSize(format);
  }

  // NOLINTNEXTLINE
  ImageData(uint32_t width, uint32_t height, Data::ByteData &byteData,
            VkFormat format)
      : Data::ByteData(byteData), width(width), height(height), format(format) {
  }
  // NOLINTNEXTLINE
  ImageData(uint32_t width, uint32_t height, VkFormat format)
      : Data::ByteData(static_cast<size_t>(width * height *
                                           Graphics::Format::GetSize(format))),
        width(width), height(height), format(format) {}

  // TODO: Refactor to return Ref, so we can refcount for lua
  static auto Create(uint32_t width, uint32_t height, VkFormat format)
      -> Result<Ref<ImageData>>;
  static auto Create(uint32_t width, uint32_t height,
                     const std::span<uint8_t> &srcData, VkFormat format)
      -> Result<Ref<ImageData>>;
  static auto Create(uint32_t width, uint32_t height, Data::ByteData &byteData,
                     VkFormat format) -> Result<Ref<ImageData>>;
  static auto Create(const std::string &filepath) -> Result<Ref<ImageData>>;
  static auto Create(const Data::ByteData &byteData) -> Result<Ref<ImageData>>;
  static auto Create(const std::span<uint8_t> &data) -> Result<Ref<ImageData>>;

  static auto GetType() -> Type const * { return &type; }

private:
  uint32_t width;
  uint32_t height;
  VkFormat format;
};

} // namespace Image