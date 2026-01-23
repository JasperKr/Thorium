#include "imagedata.hpp"
#include "Graphics/format.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/color.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include <vector>
#define VK_NO_PROTOTYPES
#include "stb/stb_image.h"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <cstring>
#include <unordered_map>

// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
// NOLINTBEGIN(hicpp-no-array-decay)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
// NOLINTBEGIN(readability-magic-numbers)

// Set, Get functions for different formats
struct FormatFunctions {
  using SetFunction = void (*)(const Color &color, uint8_t *outData);
  using GetFunction = void (*)(const uint8_t *inData, Color &outColor);

  SetFunction set;
  GetFunction get;
};

static inline auto asuint32(float floatVal) -> uint32_t {
  union {
    float floatVal;
    uint32_t u;
  } conv{};
  conv.floatVal = floatVal;
  return conv.u;
}

static inline auto asfloat32(uint32_t uintVal) -> float {
  union {
    float f;
    uint32_t uintVal;
  } conv{};
  conv.uintVal = uintVal;
  return conv.f;
}

auto float11to32(uint16_t floatVal) -> float {
  uint16_t exponent = floatVal >> 6U;
  uint16_t mantissa = floatVal & 0x3FU;

  if (exponent == 0) {
    return mantissa == 0
               ? 0
               : powf(2.0F, -14.0F) * (static_cast<float>(mantissa) / 64.0F);
  }

  if (exponent < 31U) {
    return powf(2.0F, static_cast<float>(exponent - 15U)) *
           (1.0F + static_cast<float>(mantissa) / 64.0F);
  }

  return mantissa == 0 ? std::numeric_limits<float>::infinity()
                       : std::numeric_limits<float>::quiet_NaN();
}

auto float32to11(float floatVal) -> uint16_t {
  const uint16_t EXPONENT_BITS = 0x1F;
  const uint16_t EXPONENT_SHIFT = 6;
  const uint16_t EXPONENT_BIAS = 15;
  const uint16_t MANTISSA_BITS = 0x3F;
  const uint16_t MANTISSA_SHIFT = (23 - EXPONENT_SHIFT);
  const uint16_t MAX_EXPONENT = (EXPONENT_BITS << EXPONENT_SHIFT);

  uint32_t floatAsUint = asuint32(floatVal);

  if ((floatAsUint & 0x80000000U) != 0U) {
    return 0; // Negative values go to 0.
  }

  // Map exponent to the range [-127,128]
  int32_t exponent = (int32_t)((floatAsUint >> 23U) & 0xFFU) - 127;
  uint32_t mantissa = floatAsUint & 0x007FFFFFU;

  if (exponent > 15) { // Infinity or NaN
    return MAX_EXPONENT | (exponent == 128 ? (mantissa & MANTISSA_BITS) : 0);
  }
  if (exponent <= -15) {
    return 0;
  }

  exponent += EXPONENT_BIAS;

  return (uint16_t)(static_cast<uint32_t>(exponent) << EXPONENT_SHIFT) |
         (mantissa >> MANTISSA_SHIFT);
}

auto float10to32(uint16_t floatVal) -> float {
  uint16_t exponent = floatVal >> 5U;
  uint16_t mantissa = floatVal & 0x1FU;

  if (exponent == 0U) {
    return mantissa == 0
               ? 0
               : powf(2.0F, -14.0F) * (static_cast<float>(mantissa) / 32.0F);
  }

  if (exponent < 31U) {
    return powf(2.0F, static_cast<float>(exponent) - 15U) *
           (1.0F + static_cast<float>(mantissa) / 32.0F);
  }

  return mantissa == 0 ? std::numeric_limits<float>::infinity()
                       : std::numeric_limits<float>::quiet_NaN();
}

auto float32to10(float floatVal) -> uint16_t {
  const uint16_t EXPONENT_BITS = 0x1F;
  const uint16_t EXPONENT_SHIFT = 5;
  const uint16_t EXPONENT_BIAS = 15;
  const uint16_t MANTISSA_BITS = 0x1F;
  const uint16_t MANTISSA_SHIFT = (23 - EXPONENT_SHIFT);
  const uint16_t MAX_EXPONENT = (EXPONENT_BITS << EXPONENT_SHIFT);

  uint32_t floatAsUint = asuint32(floatVal);

  if ((floatAsUint & 0x80000000U) != 0U) {
    return 0; // Negative values go to 0.
  }

  // Map exponent to the range [-127,128]
  int32_t exponent = (int32_t)((floatAsUint >> 23U) & 0xFFU) - 127;
  uint32_t mantissa = floatAsUint & 0x007FFFFFU;

  if (exponent > 15) { // Infinity or NaN
    return MAX_EXPONENT | (exponent == 128 ? (mantissa & MANTISSA_BITS) : 0);
  }

  if (exponent <= -15) {
    return 0;
  }

  exponent += EXPONENT_BIAS;

  return (static_cast<uint32_t>(exponent) << EXPONENT_SHIFT) |
         (mantissa >> MANTISSA_SHIFT);
}

static constexpr float ToUint8_t = 255.0F;
static constexpr float ToUint16_t = 65535.0F;
static constexpr float ToUint32_t = 4294967295.0F;
static constexpr float FromUint8_t = 1.0F / 255.0F;
static constexpr float FromUint16_t = 1.0F / 65535.0F;
static constexpr float FromUint32_t = 1.0F / 4294967295.0F;
// map of set and get functions for different formats
static const std::unordered_map<VkFormat, FormatFunctions> formatFunctionMap = {
    {VK_FORMAT_R8G8B8A8_UNORM,
     {// Set function
      .set = [](const Color &color, uint8_t *outData) -> void {
        outData[0] = static_cast<uint8_t>(color.r * ToUint8_t);
        outData[1] = static_cast<uint8_t>(color.g * ToUint8_t);
        outData[2] = static_cast<uint8_t>(color.b * ToUint8_t);
        outData[3] = static_cast<uint8_t>(color.a * ToUint8_t);
      },
      // Get function
      .get = [](const uint8_t *inData, Color &outColor) -> void {
        outColor.r = static_cast<float>(inData[0]) * FromUint8_t;
        outColor.g = static_cast<float>(inData[1]) * FromUint8_t;
        outColor.b = static_cast<float>(inData[2]) * FromUint8_t;
        outColor.a = static_cast<float>(inData[3]) * FromUint8_t;
      }}},
    {VK_FORMAT_R8_UNORM,
     {.set = [](const Color &color, uint8_t *outData) -> void {
        outData[0] = static_cast<uint8_t>(color.r * ToUint8_t);
      },
      .get = [](const uint8_t *inData, Color &outColor) -> void {
        outColor.r = static_cast<float>(inData[0]) * FromUint8_t;
      }}},
    {VK_FORMAT_R8G8_UNORM,
     {.set = [](const Color &color, uint8_t *outData) -> void {
        outData[0] = static_cast<uint8_t>(color.r * ToUint8_t);
        outData[1] = static_cast<uint8_t>(color.g * ToUint8_t);
      },
      .get = [](const uint8_t *inData, Color &outColor) -> void {
        outColor.r = static_cast<float>(inData[0]) * FromUint8_t;
        outColor.g = static_cast<float>(inData[1]) * FromUint8_t;
      }}},
    {VK_FORMAT_R16G16B16A16_SFLOAT,
     {.set = [](const Color &color, uint8_t *outData) -> void {
        auto r16 = static_cast<numeric::float16_t>(color.r);
        auto g16 = static_cast<numeric::float16_t>(color.g);
        auto b16 = static_cast<numeric::float16_t>(color.b);
        auto a16 = static_cast<numeric::float16_t>(color.a);
        std::memcpy(outData + 0, &r16, sizeof(numeric::float16_t));
        std::memcpy(outData + 2, &g16, sizeof(numeric::float16_t));
        std::memcpy(outData + 4, &b16, sizeof(numeric::float16_t));
        std::memcpy(outData + 6, &a16, sizeof(numeric::float16_t));
      },
      .get = [](const uint8_t *inData, Color &outColor) -> void {
        numeric::float16_t r16{};
        numeric::float16_t g16{};
        numeric::float16_t b16{};
        numeric::float16_t a16{};
        std::memcpy(&r16, inData + 0, sizeof(numeric::float16_t));
        std::memcpy(&g16, inData + 2, sizeof(numeric::float16_t));
        std::memcpy(&b16, inData + 4, sizeof(numeric::float16_t));
        std::memcpy(&a16, inData + 6, sizeof(numeric::float16_t));
        outColor.r = static_cast<float>(r16);
        outColor.g = static_cast<float>(g16);
        outColor.b = static_cast<float>(b16);
        outColor.a = static_cast<float>(a16);
      }}},
    {VK_FORMAT_R32G32B32A32_SFLOAT,
     {.set = [](const Color &color, uint8_t *outData) -> void {
        auto r32 = static_cast<float>(color.r);
        auto g32 = static_cast<float>(color.g);
        auto b32 = static_cast<float>(color.b);
        auto a32 = static_cast<float>(color.a);
        std::memcpy(outData + 0, &r32, sizeof(float));
        std::memcpy(outData + 4, &g32, sizeof(float));
        std::memcpy(outData + 8, &b32, sizeof(float));
        std::memcpy(outData + 12, &a32, sizeof(float));
      },
      .get = [](const uint8_t *inData, Color &outColor) -> void {
        float r32{};
        float g32{};
        float b32{};
        float a32{};
        std::memcpy(&r32, inData + 0, sizeof(float));
        std::memcpy(&g32, inData + 4, sizeof(float));
        std::memcpy(&b32, inData + 8, sizeof(float));
        std::memcpy(&a32, inData + 12, sizeof(float));
        outColor.r = r32;
        outColor.g = g32;
        outColor.b = b32;
        outColor.a = a32;
      }}},
    {VK_FORMAT_R32G32B32_SFLOAT,
     {.set = [](const Color &color, uint8_t *outData) -> void {
        auto r32 = static_cast<float>(color.r);
        auto g32 = static_cast<float>(color.g);
        auto b32 = static_cast<float>(color.b);
        std::memcpy(outData + 0, &r32, sizeof(float));
        std::memcpy(outData + 4, &g32, sizeof(float));
        std::memcpy(outData + 8, &b32, sizeof(float));
      },
      .get = [](const uint8_t *inData, Color &outColor) -> void {
        float r32{};
        float g32{};
        float b32{};
        std::memcpy(&r32, inData + 0, sizeof(float));
        std::memcpy(&g32, inData + 4, sizeof(float));
        std::memcpy(&b32, inData + 8, sizeof(float));
        outColor.r = r32;
        outColor.g = g32;
        outColor.b = b32;
      }}},
    {VK_FORMAT_R32_SFLOAT,
     {.set = [](const Color &color, uint8_t *outData) -> void {
        auto r32 = static_cast<float>(color.r);
        std::memcpy(outData + 0, &r32, sizeof(float));
      },
      .get = [](const uint8_t *inData, Color &outColor) -> void {
        float r32{};
        std::memcpy(&r32, inData + 0, sizeof(float));
        outColor.r = r32;
      }}},
    {VK_FORMAT_R4G4B4A4_UNORM_PACK16,
     {.set = [](const Color &color, uint8_t *outData) -> void {
        uint16_t packed = (static_cast<uint32_t>(color.r * 15.0F) << 12U) |
                          (static_cast<uint32_t>(color.g * 15.0F) << 8U) |
                          (static_cast<uint32_t>(color.b * 15.0F) << 4U) |
                          (static_cast<uint32_t>(color.a * 15.0F) << 0U);
        std::memcpy(outData, &packed, sizeof(uint16_t));
      },
      .get = [](const uint8_t *inData, Color &outColor) -> void {
        uint32_t packed{};
        std::memcpy(&packed, inData, sizeof(uint16_t));
        outColor.r = static_cast<float>((packed >> 12U) & 0x0FU) / 15.0F;
        outColor.g = static_cast<float>((packed >> 8U) & 0x0FU) / 15.0F;
        outColor.b = static_cast<float>((packed >> 4U) & 0x0FU) / 15.0F;
        outColor.a = static_cast<float>((packed >> 0U) & 0x0FU) / 15.0F;
      }}},
    {VK_FORMAT_R5G5B5A1_UNORM_PACK16,
     {.set = [](const Color &color, uint8_t *outData) -> void {
        uint16_t packed = (static_cast<uint32_t>(color.r * 31.0F) << 11U) |
                          (static_cast<uint32_t>(color.g * 31.0F) << 6U) |
                          (static_cast<uint32_t>(color.b * 31.0F) << 1U) |
                          (static_cast<uint32_t>(color.a * 1.0F) << 0U);
        std::memcpy(outData, &packed, sizeof(uint16_t));
      },
      .get = [](const uint8_t *inData, Color &outColor) -> void {
        uint32_t packed{};
        std::memcpy(&packed, inData, sizeof(uint16_t));
        outColor.r = static_cast<float>((packed >> 11U) & 0x1FU) / 31.0F;
        outColor.g = static_cast<float>((packed >> 6U) & 0x1FU) / 31.0F;
        outColor.b = static_cast<float>((packed >> 1U) & 0x1FU) / 31.0F;
        outColor.a = static_cast<float>((packed >> 0U) & 0x01U) / 1.0F;
      }}},
    {VK_FORMAT_B10G11R11_UFLOAT_PACK32,
     {.set = [](const Color &color, uint8_t *outData) -> void {
        uint32_t r10 = float32to10(static_cast<float>(color.r));
        uint32_t g11 = float32to11(static_cast<float>(color.g));
        uint32_t b10 = float32to10(static_cast<float>(color.b));
        uint32_t packed = (b10 << 22U) | (g11 << 11U) | (r10 << 0U);
        std::memcpy(outData, &packed, sizeof(uint32_t));
      },
      .get = [](const uint8_t *inData, Color &outColor) -> void {
        uint32_t packed{};
        std::memcpy(&packed, inData, sizeof(uint32_t));
        auto r10 = static_cast<uint16_t>((packed >> 0U) & 0x3FFU);
        auto g11 = static_cast<uint16_t>((packed >> 11U) & 0x7FFU);
        auto b10 = static_cast<uint16_t>((packed >> 22U) & 0x3FFU);
        outColor.r = float10to32(r10);
        outColor.g = float11to32(g11);
        outColor.b = float10to32(b10);
      }}}};

namespace Image {
auto ImageData::SetColor(Math::Uvec2 position, const Color &color) -> void {
  size_t index = (static_cast<size_t>(position.y) * static_cast<size_t>(width) +
                  static_cast<size_t>(position.x)) *
                 GetFormatSize();
  auto funcIterator = formatFunctionMap.find(format);
  if (funcIterator != formatFunctionMap.end()) {
    const FormatFunctions &functions = funcIterator->second;
    functions.set(color, &GetDataPtr()[index]);
  } else {
    // Unsupported format, handle error as needed
  }
}

auto ImageData::GetColor(Math::Uvec2 position) -> Color & {
  size_t index = (static_cast<size_t>(position.y) * static_cast<size_t>(width) +
                  static_cast<size_t>(position.x)) *
                 GetFormatSize();
  auto funcIterator = formatFunctionMap.find(format);
  static Color outColor; // NOLINT
  if (funcIterator != formatFunctionMap.end()) {
    const FormatFunctions &functions = funcIterator->second;
    functions.get(&GetDataPtr()[index], outColor);
    return outColor;
  }

  // Unsupported format, handle error as needed
  return outColor; // return default color
}

auto ImageData::Create(uint32_t width, uint32_t height, VkFormat format)
    -> Result<Ref<ImageData>> {
  return Ref<ImageData>::Make(width, height, format);
}

auto ImageData::Create(uint32_t width, uint32_t height,
                       const std::span<uint8_t> &srcData, VkFormat format)
    -> Result<Ref<ImageData>> {
  auto imgdata = Ref<ImageData>::Make(width, height, format);
  std::memcpy(imgdata->GetDataPtr(), srcData.data(), srcData.size());
  return imgdata;
}

auto ImageData::Create(uint32_t width, uint32_t height,
                       Data::ByteData &byteData, VkFormat format)
    -> Result<Ref<ImageData>> {
  size_t dataSize = static_cast<size_t>(width) * static_cast<size_t>(height) *
                    Graphics::Format::GetSize(format);
  if (byteData.GetSize() != dataSize) {
    return Error::Unexpected("ByteData size does not match image dimensions.");
  }

  return Ref<ImageData>::Make(width, height, byteData, format);
}

auto ImageData::Create(const std::string &filepath) -> Result<Ref<ImageData>> {

  auto fileLoadResult = Filesystem::ReadFile(filepath);

  if (Error::IsError(fileLoadResult)) {
    return fileLoadResult.error().AsUnexpected();
  }

  auto filedata = fileLoadResult.value();

  auto bytedata = Data::ByteData(filedata.size());
  std::memcpy(bytedata.GetData(), filedata.data(), filedata.size());

  return Create(bytedata);
}

auto ImageData::Create(const std::span<uint8_t> &data)
    -> Result<Ref<ImageData>> {
  // Allowed formats: jpeg, png, tga, bmp, psd, gif, hdr, pic, ppm

  int texWidth = 0;
  int texHeight = 0;
  int texChannels = 0;

  // check for LDR formats, supported by default stbi_load
  if (stbi_is_hdr_from_memory(data.data(), static_cast<int>(data.size())) ==
      0) {
    stbi_uc *pixels = stbi_load_from_memory(
        data.data(), static_cast<int>(data.size()), &texWidth, &texHeight,
        &texChannels, STBI_rgb_alpha);

    if (pixels == nullptr) {
      return Error::Unexpected("Failed to load image.");
    }

    auto span =
        std::span<uint8_t>(pixels, static_cast<size_t>(texWidth) *
                                       static_cast<size_t>(texHeight) * 4);

    auto imageData = Image::ImageData::Create(texWidth, texHeight, span,
                                              VK_FORMAT_R8G8B8A8_UNORM);

    stbi_image_free(pixels);

    if (Error::IsError(imageData)) {
      return imageData.error().AsUnexpected();
    }

    return imageData.value();
  }

  if (stbi_is_hdr_from_memory(data.data(), static_cast<int>(data.size())) !=
      0) {
    float *pixels = stbi_loadf_from_memory(
        data.data(), static_cast<int>(data.size()), &texWidth, &texHeight,
        &texChannels, STBI_rgb_alpha); // force 4 channels
    if (pixels == nullptr) {
      return Error::Unexpected("Failed to load image.");
    }

    // NOLINTNEXTLINE, reinterpret cast is safe here
    const auto span = std::span<uint8_t>(reinterpret_cast<uint8_t *>(pixels),
                                         static_cast<size_t>(texWidth) *
                                             static_cast<size_t>(texHeight) *
                                             4 * sizeof(float));

    auto imageData = Image::ImageData::Create(texWidth, texHeight, span,
                                              VK_FORMAT_R32G32B32A32_SFLOAT);

    stbi_image_free(pixels);

    if (Error::IsError(imageData)) {
      return imageData.error().AsUnexpected();
    }

    return imageData.value();
  }

  return Error::Unexpected("Unsupported image format.");
}

auto ImageData::Create(const Data::ByteData &byteData)
    -> Result<Ref<ImageData>> {
  return Create(byteData.GetDataSpan());
}

} // namespace Image

// NOLINTEND(readability-magic-numbers)
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTEND(hicpp-no-array-decay)
// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
// NOLINTEND(cppcoreguidelines-pro-type-union-access)