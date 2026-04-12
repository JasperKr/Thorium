#pragma once

#include "../Graphics/format.hpp"
#include "Math/vector.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "color.hpp"
#include <cassert>
#include <cstddef>
#include <span>

#include "vulkan/vulkan_core.h"
#include <cstdint>
namespace Image {

static const Type LuaImageDataType = Type("ImageData");

struct ImageData : Object {
public:
  ~ImageData() override { internalData = {}; }

  auto SetColor(Math::Uvec2 position, const Color &color) -> Error;
  auto GetColor(Math::Uvec2 position) -> Result<Color>;
  auto Copy(const ImageData &source) -> void;
  auto GetDataPtr() -> uint8_t * { return internalData->GetData(); }
  [[nodiscard]] auto GetSize() const -> size_t {
    return internalData->GetSize();
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

  ImageData(const ImageData &) = delete;
  ImageData(ImageData &&) = delete;
  auto operator=(const ImageData &) -> ImageData & = delete;
  auto operator=(ImageData &&) -> ImageData & = delete;
  // NOLINTNEXTLINE
  ImageData(uint32_t width, uint32_t height, Data::ByteData &byteData,
            VkFormat format)
      : internalData(&byteData), width(width), height(height), format(format) {}
  // NOLINTNEXTLINE
  ImageData(uint32_t width, uint32_t height, VkFormat format)
      : internalData(Ref<Data::ByteData>::Make(static_cast<size_t>(
            width * height * Graphics::Format::GetSize(format)))),
        width(width), height(height), format(format) {}

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

  static auto GetType() -> Type const * { return &LuaImageDataType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return ImageData::GetType();
  }

  auto GetPitch() const -> size_t {
    return width * static_cast<size_t>(Graphics::Format::GetSize(format));
  }

private:
  uint32_t width;
  uint32_t height;
  VkFormat format;

  Ref<Data::ByteData> internalData;
};

} // namespace Image