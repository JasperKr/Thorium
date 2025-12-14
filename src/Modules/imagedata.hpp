#pragma once

#include "Modules/bytedata.hpp"
#include "Modules/image.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "color.hpp"
#include "vector.hpp"
#include "vulkan/vulkan_core.h"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace Image {

static const Type type = Type("ImageData");

struct ImageData : Object {
public:
  auto SetColor(Uvec2 position, const Color &color) -> void;
  auto GetColor(Uvec2 position) -> Color &;
  auto Copy(const ImageData &source) -> void;
  auto GetDataPtr() -> uint8_t * { return data.data(); }
  [[nodiscard]] auto GetSize() const -> size_t { return data.size(); }
  auto GetData() -> const std::vector<uint8_t> & { return data; }
  [[nodiscard]] auto GetWidth() const -> uint32_t { return width; }
  [[nodiscard]] auto GetHeight() const -> uint32_t { return height; }
  [[nodiscard]] auto GetFormat() const -> VkFormat { return format; }
  [[nodiscard]] auto GetChannelCount() const -> uint32_t {
    return Image::GetFormatChannelCount(format);
  }
  [[nodiscard]] auto GetFormatSize() const -> size_t {
    return Image::GetFormatSize(format);
  }

  // NOLINTNEXTLINE
  ImageData(uint32_t width, uint32_t height, VkFormat format)
      : width(width), height(height), format(format),
        data(static_cast<size_t>(width * height * Image::GetFormatSize(format)),
             0) {}
  // NOLINTNEXTLINE
  ImageData(uint32_t width, uint32_t height, const uint8_t *srcData,
            size_t dataSize)
      : width(width), height(height), format(VK_FORMAT_R8G8B8A8_UNORM),
        data(srcData, srcData + dataSize) {} // NOLINT
  // NOLINTNEXTLINE
  ImageData(uint32_t width, uint32_t height,
            const std::vector<uint8_t> &srcData)
      : width(width), height(height), format(VK_FORMAT_R8G8B8A8_UNORM),
        data(srcData) {} // NOLINT
                         // NOLINTNEXTLINE
  ImageData(uint32_t width, uint32_t height, Data::ByteData &byteData)
      : width(width), height(height), format(VK_FORMAT_R8G8B8A8_UNORM),
        data(byteData.GetSize()) {
    std::memcpy(data.data(), byteData.GetData(), byteData.GetSize());
  }

  static auto GetType() -> const Type * { return &type; }

  auto ScheduleDestroy() -> bool override { return false; };

private:
  uint32_t width;
  uint32_t height;
  VkFormat format;
  std::vector<uint8_t> data;
};

} // namespace Image