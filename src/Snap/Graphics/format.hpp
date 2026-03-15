#pragma once
#include <cstddef>

#include <string>
#include <vulkan/vulkan_core.h>
namespace Graphics::Format {
static inline auto GetChannelCount(VkFormat format) -> uint32_t {
  switch (format) {
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8_SRGB:
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16_SINT:
  case VK_FORMAT_R16_SFLOAT:
  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32_SINT:
  case VK_FORMAT_R32_SFLOAT:
    return 1;

  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8_SRGB:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16G16_SFLOAT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32G32_SFLOAT:
    return 2;

  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8_SRGB:
  case VK_FORMAT_B8G8R8_UNORM:
  case VK_FORMAT_B8G8R8_SRGB:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16_SFLOAT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32B32_SFLOAT:
    return 3;

  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_B8G8R8A8_UNORM:
  case VK_FORMAT_B8G8R8A8_SRGB:
  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16A16_SNORM:
  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R16G16B16A16_SINT:
  case VK_FORMAT_R16G16B16A16_SFLOAT:
  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R32G32B32A32_SINT:
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return 4;
  default:
    return 0;
  }
}

static inline auto GetSize(VkFormat format) -> uint32_t {
  constexpr uint32_t byteSize = 1;
  constexpr uint32_t shortSize = 2;
  constexpr uint32_t halfSize = 2; // FP16
  constexpr uint32_t floatSize = 4;
  constexpr uint32_t intSize = 4;

  constexpr uint32_t BC1BlockSize = 8;
  constexpr uint32_t BC2BlockSize = 16;
  constexpr uint32_t BC3BlockSize = 16;
  constexpr uint32_t BC4BlockSize = 8;
  constexpr uint32_t BC5BlockSize = 16;
  constexpr uint32_t BC6HBlockSize = 16;
  constexpr uint32_t BC7BlockSize = 16;

  switch (format) {
  //
  // --- 1 CHANNEL ---
  //
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8_SRGB:
    return byteSize * 1;

  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16_SINT:
    return shortSize * 1;

  case VK_FORMAT_R16_SFLOAT:
    return halfSize * 1;

  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32_SINT:
  case VK_FORMAT_R32_SFLOAT:
    return floatSize * 1;

  //
  // --- 2 CHANNEL ---
  //
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8_SRGB:
    return byteSize * 2;

  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16_SINT:
    return shortSize * 2;

  case VK_FORMAT_R16G16_SFLOAT:
    return halfSize * 2;

  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32G32_SFLOAT:
    return floatSize * 2;

  //
  // --- 3 CHANNEL ---
  //
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8_SRGB:
  case VK_FORMAT_B8G8R8_UNORM:
  case VK_FORMAT_B8G8R8_SRGB:
    return byteSize * 3;

  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16_SFLOAT:
    return shortSize * 3;

  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32B32_SFLOAT:
    return floatSize * 3;

  //
  // --- 4 CHANNEL ---
  //
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_R8G8B8A8_SNORM:
  case VK_FORMAT_B8G8R8A8_UNORM:
  case VK_FORMAT_B8G8R8A8_SRGB:
  case VK_FORMAT_R8G8B8A8_UINT:
  case VK_FORMAT_R8G8B8A8_SINT:
    return byteSize * 4;

  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16A16_SNORM:
  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R16G16B16A16_SINT:
    return shortSize * 4;

  case VK_FORMAT_R16G16B16A16_SFLOAT:
    return halfSize * 4;

  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R32G32B32A32_SINT:
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return floatSize * 4;

  //
  // --- Depth / Stencil ---
  //
  case VK_FORMAT_D16_UNORM:
    return shortSize;
  case VK_FORMAT_D24_UNORM_S8_UINT:
    return (3 * byteSize) + byteSize;
  case VK_FORMAT_X8_D24_UNORM_PACK32:
    return intSize;
  case VK_FORMAT_D32_SFLOAT:
    return floatSize;
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return floatSize + byteSize;

  //
  // --- BC Compressed (block size) ---
  //
  case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
  case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
  case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
  case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    return BC1BlockSize;

  case VK_FORMAT_BC2_UNORM_BLOCK:
  case VK_FORMAT_BC2_SRGB_BLOCK:
    return BC2BlockSize;
  case VK_FORMAT_BC3_UNORM_BLOCK:
  case VK_FORMAT_BC3_SRGB_BLOCK:
    return BC3BlockSize;

  case VK_FORMAT_BC4_UNORM_BLOCK:
  case VK_FORMAT_BC4_SNORM_BLOCK:
    return BC4BlockSize;

  case VK_FORMAT_BC5_UNORM_BLOCK:
  case VK_FORMAT_BC5_SNORM_BLOCK:
    return BC5BlockSize;

  case VK_FORMAT_BC6H_UFLOAT_BLOCK:
  case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    return BC6HBlockSize;
  case VK_FORMAT_BC7_UNORM_BLOCK:
  case VK_FORMAT_BC7_SRGB_BLOCK:
    return BC7BlockSize;

  default:
    return 0;
  }
}

// NOLINTNEXTLINE
static inline auto StringToImageFormat(const std::string &format) -> VkFormat {
  // 8-bit unorm
  if (format == "rgba8") {
    return VK_FORMAT_R8G8B8A8_UNORM;
  }
  if (format == "rg8") {
    return VK_FORMAT_R8G8_UNORM;
  }
  if (format == "r8") {
    return VK_FORMAT_R8_UNORM;
  }
  // 16-bit unorm
  if (format == "rgba16") {
    return VK_FORMAT_R16G16B16A16_UNORM;
  }
  if (format == "rg16") {
    return VK_FORMAT_R16G16_UNORM;
  }
  if (format == "r16") {
    return VK_FORMAT_R16_UNORM;
  }
  // 16-bit float
  if (format == "rgba16f") {
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  }
  if (format == "rg16f") {
    return VK_FORMAT_R16G16_SFLOAT;
  }
  if (format == "r16f") {
    return VK_FORMAT_R16_SFLOAT;
  }
  // 32-bit float
  if (format == "rgba32f") {
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  }
  if (format == "rg32f") {
    return VK_FORMAT_R32G32_SFLOAT;
  }
  if (format == "r32f") {
    return VK_FORMAT_R32_SFLOAT;
  }
  // 8-bit uint
  if (format == "rgba8ui") {
    return VK_FORMAT_R8G8B8A8_UINT;
  }
  if (format == "rg8ui") {
    return VK_FORMAT_R8G8_UINT;
  }
  if (format == "r8ui") {
    return VK_FORMAT_R8_UINT;
  }
  // 16-bit uint
  if (format == "rgba16ui") {
    return VK_FORMAT_R16G16B16A16_UINT;
  }
  if (format == "rg16ui") {
    return VK_FORMAT_R16G16_UINT;
  }
  if (format == "r16ui") {
    return VK_FORMAT_R16_UINT;
  }
  // 32-bit uint
  if (format == "rgba32ui") {
    return VK_FORMAT_R32G32B32A32_UINT;
  }
  if (format == "rg32ui") {
    return VK_FORMAT_R32G32_UINT;
  }
  if (format == "r32ui") {
    return VK_FORMAT_R32_UINT;
  }
  // 8-bit sint
  if (format == "rgba8si") {
    return VK_FORMAT_R8G8B8A8_SINT;
  }
  if (format == "rg8si") {
    return VK_FORMAT_R8G8_SINT;
  }
  if (format == "r8si") {
    return VK_FORMAT_R8_SINT;
  }
  // 16-bit sint
  if (format == "rgba16si") {
    return VK_FORMAT_R16G16B16A16_SINT;
  }
  if (format == "rg16si") {
    return VK_FORMAT_R16G16_SINT;
  }
  if (format == "r16si") {
    return VK_FORMAT_R16_SINT;
  }
  // 32-bit sint
  if (format == "rgba32si") {
    return VK_FORMAT_R32G32B32A32_SINT;
  }
  if (format == "rg32si") {
    return VK_FORMAT_R32G32_SINT;
  }
  if (format == "r32si") {
    return VK_FORMAT_R32_SINT;
  }
  // Depth formats
  if (format == "depth16") {
    return VK_FORMAT_D16_UNORM;
  }
  if (format == "depth24") {
    return VK_FORMAT_X8_D24_UNORM_PACK32;
  }
  if (format == "depth32") {
    return VK_FORMAT_D32_SFLOAT;
  }
  // Depth-stencil formats
  if (format == "depth24stencil8") {
    return VK_FORMAT_D24_UNORM_S8_UINT;
  }
  if (format == "depth32stencil8") {
    return VK_FORMAT_D32_SFLOAT_S8_UINT;
  }
  // packed formats
  if (format == "rg11b10f") {
    return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
  }
  if (format == "rgb9e5") {
    return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
  }
  if (format == "rgb10a2") {
    return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
  }
  if (format == "rgb10a2ui") {
    return VK_FORMAT_A2B10G10R10_UINT_PACK32;
  }
  if (format == "bgr5a1") {
    return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
  }
  if (format == "bgr565") {
    return VK_FORMAT_R5G6B5_UNORM_PACK16;
  }
  if (format == "rgba4") {
    return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
  }
  // compressed formats
  if (format == "bc1") {
    return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
  }
  if (format == "bc3") {
    return VK_FORMAT_BC3_UNORM_BLOCK;
  }
  if (format == "bc4") {
    return VK_FORMAT_BC4_UNORM_BLOCK;
  }
  if (format == "bc5") {
    return VK_FORMAT_BC5_UNORM_BLOCK;
  }
  if (format == "bc6h") {
    return VK_FORMAT_BC6H_UFLOAT_BLOCK;
  }
  if (format == "bc6hs") {
    return VK_FORMAT_BC6H_SFLOAT_BLOCK;
  }
  if (format == "bc7") {
    return VK_FORMAT_BC7_UNORM_BLOCK;
  }

  return VK_FORMAT_UNDEFINED;
}

// NOLINTNEXTLINE
static inline auto ImageFormatToString(VkFormat format) -> std::string {
  switch (format) {
  case VK_FORMAT_R8G8B8A8_UNORM:
    return "rgba8";
  case VK_FORMAT_R8G8B8A8_SRGB:
    return "srgba8";
  case VK_FORMAT_R16G16B16A16_UNORM:
    return "rgba16";
  case VK_FORMAT_R16G16B16A16_SFLOAT:
    return "rgba16f";
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return "rgba32f";
  case VK_FORMAT_R8G8B8A8_UINT:
    return "rgba8ui";
  case VK_FORMAT_R16G16B16A16_UINT:
    return "rgba16ui";
  case VK_FORMAT_R32G32B32A32_UINT:
    return "rgba32ui";
  case VK_FORMAT_R8G8B8A8_SINT:
    return "rgba8si";
  case VK_FORMAT_R16G16B16A16_SINT:
    return "rgba16si";
  case VK_FORMAT_R32G32B32A32_SINT:
    return "rgba32si";
  case VK_FORMAT_D16_UNORM:
    return "depth16";
  case VK_FORMAT_X8_D24_UNORM_PACK32:
    return "depth24";
  case VK_FORMAT_D32_SFLOAT:
    return "depth32";
  case VK_FORMAT_D24_UNORM_S8_UINT:
    return "depth24stencil8";
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return "depth32stencil8";
  case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    return "rg11b10f";
  case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
    return "rgb9e5";
  case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    return "rgb10a2";
  case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    return "rgb10a2ui";
  case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
    return "bgr5a1";
  case VK_FORMAT_R5G6B5_UNORM_PACK16:
    return "bgr565";
  case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    return "rgba4";
  case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    return "bc1";
  case VK_FORMAT_BC3_UNORM_BLOCK:
    return "bc3";
  case VK_FORMAT_BC4_UNORM_BLOCK:
    return "bc4";
  case VK_FORMAT_BC5_UNORM_BLOCK:
    return "bc5";
  case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    return "bc6h";
  case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    return "bc6hs";
  case VK_FORMAT_BC7_UNORM_BLOCK:
    return "bc7";
  default:
    return "undefined";
  }
}

static auto ToString(VkFormat format, size_t arraySize = 1) -> std::string {
  switch (format) {
  case VK_FORMAT_R32_SFLOAT:
    return "float";
  case VK_FORMAT_R32G32_SFLOAT:
    if (arraySize == 1) {
      return "floatvec2";
    } else if (arraySize == 2) {
      return "floatmat2";
    } else if (arraySize == 3) {
      return "floatmat2x3";
    } else if (arraySize == 4) {
      return "floatmat2x4";
    }
    return "unknown";
  case VK_FORMAT_R32G32B32_SFLOAT:
    if (arraySize == 1) {
      return "floatvec3";
    } else if (arraySize == 2) {
      return "floatmat3x2";
    } else if (arraySize == 3) {
      return "floatmat3";
    } else if (arraySize == 4) {
      return "floatmat3x4";
    }
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    if (arraySize == 1) {
      return "floatvec4";
    } else if (arraySize == 2) {
      return "floatmat4x2";
    } else if (arraySize == 3) {
      return "floatmat4x3";
    } else if (arraySize == 4) {
      return "floatmat4";
    }
  case VK_FORMAT_R16_SFLOAT:
    return "half";
  case VK_FORMAT_R16G16_SFLOAT:
    return "halfvec2";
  case VK_FORMAT_R16G16B16_SFLOAT:
    return "halfvec3";
  case VK_FORMAT_R16G16B16A16_SFLOAT:
    return "halfvec4";
  case VK_FORMAT_R8_UINT:
    return "uint8";
  case VK_FORMAT_R8G8_UINT:
    return "uint8vec2";
  case VK_FORMAT_R8G8B8_UINT:
    return "uint8vec3";
  case VK_FORMAT_R8G8B8A8_UINT:
    return "uint8vec4";
  case VK_FORMAT_R16_UINT:
    return "uint16";
  case VK_FORMAT_R16G16_UINT:
    return "uint16vec2";
  case VK_FORMAT_R16G16B16_UINT:
    return "uint16vec3";
  case VK_FORMAT_R16G16B16A16_UINT:
    return "uint16vec4";
  case VK_FORMAT_R32_UINT:
    return "uint32";
  case VK_FORMAT_R32G32_UINT:
    return "uint32vec2";
  case VK_FORMAT_R32G32B32_UINT:
    return "uint32vec3";
  case VK_FORMAT_R32G32B32A32_UINT:
    return "uint32vec4";
  case VK_FORMAT_R8_SINT:
    return "int8";
  case VK_FORMAT_R8G8_SINT:
    return "int8vec2";
  case VK_FORMAT_R8G8B8_SINT:
    return "int8vec3";
  case VK_FORMAT_R8G8B8A8_SINT:
    return "int8vec4";
  case VK_FORMAT_R16_SINT:
    return "int16";
  case VK_FORMAT_R16G16_SINT:
    return "int16vec2";
  case VK_FORMAT_R16G16B16_SINT:
    return "int16vec3";
  case VK_FORMAT_R16G16B16A16_SINT:
    return "int16vec4";
  case VK_FORMAT_R32_SINT:
    return "int32";
  case VK_FORMAT_R32G32_SINT:
    return "int32vec2";
  case VK_FORMAT_R32G32B32_SINT:
    return "int32vec3";
  case VK_FORMAT_R32G32B32A32_SINT:
    return "int32vec4";
  case VK_FORMAT_R8_UNORM:
    return "unorm8";
  case VK_FORMAT_R8G8_UNORM:
    return "unorm8vec2";
  case VK_FORMAT_R8G8B8_UNORM:
    return "unorm8vec3";
  case VK_FORMAT_R8G8B8A8_UNORM:
    return "unorm8vec4";
  case VK_FORMAT_R16_UNORM:
    return "unorm16";
  case VK_FORMAT_R16G16_UNORM:
    return "unorm16vec2";
  case VK_FORMAT_R16G16B16_UNORM:
    return "unorm16vec3";
  case VK_FORMAT_R16G16B16A16_UNORM:
    return "unorm16vec4";
  case VK_FORMAT_R8_SNORM:
    return "snorm8";
  case VK_FORMAT_R8G8_SNORM:
    return "snorm8vec2";
  case VK_FORMAT_R8G8B8_SNORM:
    return "snorm8vec3";
  case VK_FORMAT_R8G8B8A8_SNORM:
    return "snorm8vec4";
  case VK_FORMAT_R16_SNORM:
    return "snorm16";
  case VK_FORMAT_R16G16_SNORM:
    return "snorm16vec2";
  case VK_FORMAT_R16G16B16_SNORM:
    return "snorm16vec3";
  case VK_FORMAT_R16G16B16A16_SNORM:
    return "snorm16vec4";
  default:
    return "unknown";
  }
}

// NOLINTNEXTLINE, cognitive complexity
static auto FromString(const std::string &format) -> VkFormat {
  if (format == "unknown") {
    return VK_FORMAT_UNDEFINED;
  }
  if (format == "float") {
    return VK_FORMAT_R32_SFLOAT;
  }
  if (format == "floatvec2") {
    return VK_FORMAT_R32G32_SFLOAT;
  }
  if (format == "floatvec3") {
    return VK_FORMAT_R32G32B32_SFLOAT;
  }
  if (format == "floatvec4") {
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  }
  if (format == "half") {
    return VK_FORMAT_R16_SFLOAT;
  }
  if (format == "halfvec2") {
    return VK_FORMAT_R16G16_SFLOAT;
  }
  if (format == "halfvec3") {
    return VK_FORMAT_R16G16B16_SFLOAT;
  }
  if (format == "halfvec4") {
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  }
  if (format == "uint8") {
    return VK_FORMAT_R8_UINT;
  }
  if (format == "uint8vec2") {
    return VK_FORMAT_R8G8_UINT;
  }
  if (format == "uint8vec3") {
    return VK_FORMAT_R8G8B8_UINT;
  }
  if (format == "uint8vec4") {
    return VK_FORMAT_R8G8B8A8_UINT;
  }
  if (format == "uint16") {
    return VK_FORMAT_R16_UINT;
  }
  if (format == "uint16vec2") {
    return VK_FORMAT_R16G16_UINT;
  }
  if (format == "uint16vec3") {
    return VK_FORMAT_R16G16B16_UINT;
  }
  if (format == "uint16vec4") {
    return VK_FORMAT_R16G16B16A16_UINT;
  }
  if (format == "uint32") {
    return VK_FORMAT_R32_UINT;
  }
  if (format == "uint32vec2") {
    return VK_FORMAT_R32G32_UINT;
  }
  if (format == "uint32vec3") {
    return VK_FORMAT_R32G32B32_UINT;
  }
  if (format == "uint32vec4") {
    return VK_FORMAT_R32G32B32A32_UINT;
  }
  if (format == "int8") {
    return VK_FORMAT_R8_SINT;
  }
  if (format == "int8vec2") {
    return VK_FORMAT_R8G8_SINT;
  }
  if (format == "int8vec3") {
    return VK_FORMAT_R8G8B8_SINT;
  }
  if (format == "int8vec4") {
    return VK_FORMAT_R8G8B8A8_SINT;
  }
  if (format == "int16") {
    return VK_FORMAT_R16_SINT;
  }
  if (format == "int16vec2") {
    return VK_FORMAT_R16G16_SINT;
  }
  if (format == "int16vec3") {
    return VK_FORMAT_R16G16B16_SINT;
  }
  if (format == "int16vec4") {
    return VK_FORMAT_R16G16B16A16_SINT;
  }
  if (format == "int32") {
    return VK_FORMAT_R32_SINT;
  }
  if (format == "int32vec2") {
    return VK_FORMAT_R32G32_SINT;
  }
  if (format == "int32vec3") {
    return VK_FORMAT_R32G32B32_SINT;
  }
  if (format == "int32vec4") {
    return VK_FORMAT_R32G32B32A32_SINT;
  }
  if (format == "unorm8") {
    return VK_FORMAT_R8_UNORM;
  }
  if (format == "unorm8vec2") {
    return VK_FORMAT_R8G8_UNORM;
  }
  if (format == "unorm8vec3") {
    return VK_FORMAT_R8G8B8_UNORM;
  }
  if (format == "unorm8vec4") {
    return VK_FORMAT_R8G8B8A8_UNORM;
  }
  if (format == "unorm16") {
    return VK_FORMAT_R16_UNORM;
  }
  if (format == "unorm16vec2") {
    return VK_FORMAT_R16G16_UNORM;
  }
  if (format == "unorm16vec3") {
    return VK_FORMAT_R16G16B16_UNORM;
  }
  if (format == "unorm16vec4") {
    return VK_FORMAT_R16G16B16A16_UNORM;
  }
  if (format == "snorm8") {
    return VK_FORMAT_R8_SNORM;
  }
  if (format == "snorm8vec2") {
    return VK_FORMAT_R8G8_SNORM;
  }
  if (format == "snorm8vec3") {
    return VK_FORMAT_R8G8B8_SNORM;
  }
  if (format == "snorm8vec4") {
    return VK_FORMAT_R8G8B8A8_SNORM;
  }
  if (format == "snorm16") {
    return VK_FORMAT_R16_SNORM;
  }
  if (format == "snorm16vec2") {
    return VK_FORMAT_R16G16_SNORM;
  }
  if (format == "snorm16vec3") {
    return VK_FORMAT_R16G16B16_SNORM;
  }
  if (format == "snorm16vec4") {
    return VK_FORMAT_R16G16B16A16_SNORM;
  }
  if (format == "floatmat2" || format == "floatmat2x2") {
    return VK_FORMAT_R32G32_SFLOAT;
  }
  if (format == "floatmat3" || format == "floatmat3x3") {
    return VK_FORMAT_R32G32B32_SFLOAT;
  }
  if (format == "floatmat4" || format == "floatmat4x4") {
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  }
  if (format == "halfmat2" || format == "halfmat2x2") {
    return VK_FORMAT_R16G16_SFLOAT;
  }
  if (format == "halfmat3" || format == "halfmat3x3") {
    return VK_FORMAT_R16G16B16_SFLOAT;
  }
  if (format == "halfmat4" || format == "halfmat4x4") {
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  }
  if (format == "floatmat2x3") {
    return VK_FORMAT_R32G32B32_SFLOAT;
  }
  if (format == "floatmat3x2") {
    return VK_FORMAT_R32G32_SFLOAT;
  }
  if (format == "floatmat2x4") {
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  }
  if (format == "floatmat4x2") {
    return VK_FORMAT_R32G32_SFLOAT;
  }
  if (format == "floatmat3x4") {
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  }
  if (format == "floatmat4x3") {
    return VK_FORMAT_R32G32B32_SFLOAT;
  }
  if (format == "halfmat2x3") {
    return VK_FORMAT_R16G16B16_SFLOAT;
  }
  if (format == "halfmat3x2") {
    return VK_FORMAT_R16G16_SFLOAT;
  }
  if (format == "halfmat2x4") {
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  }
  if (format == "halfmat4x2") {
    return VK_FORMAT_R16G16_SFLOAT;
  }
  if (format == "halfmat3x4") {
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  }
  if (format == "halfmat4x3") {
    return VK_FORMAT_R16G16B16_SFLOAT;
  }

  return VK_FORMAT_UNDEFINED;
}

static auto StringToArraySize(const std::string &format) -> size_t {
  if (format == "floatmat2x2" || format == "floatmat2") {
    return 2;
  }
  if (format == "floatmat3x3" || format == "floatmat3") {
    return 3;
  }
  if (format == "floatmat4x4" || format == "floatmat4") {
    return 4;
  }
  if (format == "floatmat2x3") {
    return 3;
  }
  if (format == "floatmat3x2") {
    return 2;
  }
  if (format == "floatmat2x4") {
    return 4;
  }
  if (format == "floatmat4x2") {
    return 2;
  }
  if (format == "floatmat3x4") {
    return 4;
  }
  if (format == "floatmat4x3") {
    return 3;
  }
  if (format == "halfmat2x3") {
    return 3;
  }
  if (format == "halfmat3x2") {
    return 2;
  }
  if (format == "halfmat2x4") {
    return 4;
  }
  if (format == "halfmat4x2") {
    return 2;
  }
  if (format == "halfmat3x4") {
    return 4;
  }
  if (format == "halfmat4x3") {
    return 3;
  }

  return 1; // default array size is 1 for non-matrix types
}

static auto GetVec4Variant(VkFormat format) -> VkFormat {
  switch (format) {
  case VK_FORMAT_R32_SFLOAT:
  case VK_FORMAT_R32G32_SFLOAT:
  case VK_FORMAT_R32G32B32_SFLOAT:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case VK_FORMAT_R16_SFLOAT:
  case VK_FORMAT_R16G16_SFLOAT:
  case VK_FORMAT_R16G16B16_SFLOAT:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8B8_UINT:
    return VK_FORMAT_R8G8B8A8_UINT;
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16B16_UINT:
    return VK_FORMAT_R16G16B16A16_UINT;
  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32B32_UINT:
    return VK_FORMAT_R32G32B32A32_UINT;
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8B8_SINT:
    return VK_FORMAT_R8G8B8A8_SINT;
  case VK_FORMAT_R16_SINT:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16G16B16_SINT:
    return VK_FORMAT_R16G16B16A16_SINT;
  case VK_FORMAT_R32_SINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32G32B32_SINT:
    return VK_FORMAT_R32G32B32A32_SINT;
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8B8_UNORM:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16B16_UNORM:
    return VK_FORMAT_R16G16B16A16_UNORM;
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8B8_SNORM:
    return VK_FORMAT_R8G8B8A8_SNORM;
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16B16_SNORM:
    return VK_FORMAT_R16G16B16A16_SNORM;
  default:
    return format; // If no specific vec4 variant, return the original format
  }
}

} // namespace Graphics::Format