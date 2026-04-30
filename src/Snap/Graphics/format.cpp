#include "format.hpp"
#include "Modules/console.hpp"
#include "float16_t/float16_t.hpp"
#include <cassert>
#include <unordered_map>

namespace Graphics::Format {

auto GetChannelCount(VkFormat format) -> uint32_t {
  static const std::unordered_map<VkFormat, uint8_t> FormatChannelCounts = {
      {VK_FORMAT_R8_UNORM, 1},
      {VK_FORMAT_R8_SNORM, 1},
      {VK_FORMAT_R8_UINT, 1},
      {VK_FORMAT_R8_SINT, 1},
      {VK_FORMAT_R8_SRGB, 1},
      {VK_FORMAT_R16_UNORM, 1},
      {VK_FORMAT_R16_SNORM, 1},
      {VK_FORMAT_R16_UINT, 1},
      {VK_FORMAT_R16_SINT, 1},
      {VK_FORMAT_R16_SFLOAT, 1},
      {VK_FORMAT_R32_UINT, 1},
      {VK_FORMAT_R32_SINT, 1},
      {VK_FORMAT_R32_SFLOAT, 1},

      {VK_FORMAT_R8G8_UNORM, 2},
      {VK_FORMAT_R8G8_SNORM, 2},
      {VK_FORMAT_R8G8_UINT, 2},
      {VK_FORMAT_R8G8_SINT, 2},
      {VK_FORMAT_R8G8_SRGB, 2},
      {VK_FORMAT_R16G16_UNORM, 2},
      {VK_FORMAT_R16G16_SNORM, 2},
      {VK_FORMAT_R16G16_UINT, 2},
      {VK_FORMAT_R16G16_SINT, 2},
      {VK_FORMAT_R16G16_SFLOAT, 2},
      {VK_FORMAT_R32G32_UINT, 2},
      {VK_FORMAT_R32G32_SINT, 2},
      {VK_FORMAT_R32G32_SFLOAT, 2},

      {VK_FORMAT_R8G8B8_UNORM, 3},
      {VK_FORMAT_R8G8B8_SNORM, 3},
      {VK_FORMAT_R8G8B8_SRGB, 3},
      {VK_FORMAT_B8G8R8_UNORM, 3},
      {VK_FORMAT_B8G8R8_SRGB, 3},
      {VK_FORMAT_B8G8R8_SRGB, 3},
      {VK_FORMAT_R16G16B16_UNORM, 3},
      {VK_FORMAT_R16G16B16_SNORM, 3},
      {VK_FORMAT_R16G16B16_UINT, 3},
      {VK_FORMAT_R16G16B16_SINT, 3},
      {VK_FORMAT_R16G16B16_SFLOAT, 3},
      {VK_FORMAT_R32G32B32_UINT, 3},
      {VK_FORMAT_R32G32B32_SINT, 3},
      {VK_FORMAT_R32G32B32_SFLOAT, 3},

      {VK_FORMAT_R8G8B8A8_UNORM, 4},
      {VK_FORMAT_R8G8B8A8_SRGB, 4},
      {VK_FORMAT_B8G8R8A8_UNORM, 4},
      {VK_FORMAT_B8G8R8A8_SRGB, 4},
      {VK_FORMAT_R16G16B16A16_UNORM, 4},
      {VK_FORMAT_R16G16B16A16_SNORM, 4},
      {VK_FORMAT_R16G16B16A16_UINT, 4},
      {VK_FORMAT_R16G16B16A16_SINT, 4},
      {VK_FORMAT_R16G16B16A16_SFLOAT, 4},
      {VK_FORMAT_R32G32B32A32_UINT, 4},
      {VK_FORMAT_R32G32B32A32_SINT, 4},
      {VK_FORMAT_R32G32B32A32_SFLOAT, 4},
  };

  auto iter = FormatChannelCounts.find(format);
  if (iter != FormatChannelCounts.end()) {
    return iter->second;
  }

  auto formatStr = ToString(format, 1);
  PrintError("Unknown format {} for channel count retrieval.", formatStr);

  return 0;
}

auto GetSize(VkFormat format) -> uint32_t {
  static const std::unordered_map<VkFormat, uint32_t> FormatSizes = {
      // 1 channel
      {VK_FORMAT_R8_UNORM, 1},
      {VK_FORMAT_R8_SNORM, 1},
      {VK_FORMAT_R8_UINT, 1},
      {VK_FORMAT_R8_SINT, 1},
      {VK_FORMAT_R8_SRGB, 1},
      {VK_FORMAT_R16_UNORM, 2},
      {VK_FORMAT_R16_SNORM, 2},
      {VK_FORMAT_R16_UINT, 2},
      {VK_FORMAT_R16_SINT, 2},
      {VK_FORMAT_R16_SFLOAT, 2},
      {VK_FORMAT_R32_UINT, 4},
      {VK_FORMAT_R32_SINT, 4},
      {VK_FORMAT_R32_SFLOAT, 4},
      // 2 channel
      {VK_FORMAT_R8G8_UNORM, 2},
      {VK_FORMAT_R8G8_SNORM, 2},
      {VK_FORMAT_R8G8_UINT, 2},
      {VK_FORMAT_R8G8_SINT, 2},
      {VK_FORMAT_R8G8_SRGB, 2},
      {VK_FORMAT_R16G16_UNORM, 4},
      {VK_FORMAT_R16G16_SNORM, 4},
      {VK_FORMAT_R16G16_UINT, 4},
      {VK_FORMAT_R16G16_SINT, 4},
      {VK_FORMAT_R16G16_SFLOAT, 4},
      {VK_FORMAT_R32G32_UINT, 8},
      {VK_FORMAT_R32G32_SINT, 8},
      {VK_FORMAT_R32G32_SFLOAT, 8},
      // 3 channel
      {VK_FORMAT_R8G8B8_UNORM, 3},
      {VK_FORMAT_R8G8B8_SRGB, 3},
      {VK_FORMAT_B8G8R8_UNORM, 3},
      {VK_FORMAT_B8G8R8_SRGB, 3},
      {VK_FORMAT_R16G16B16_UNORM, 6},
      {VK_FORMAT_R16G16B16_SNORM, 6},
      {VK_FORMAT_R16G16B16_UINT, 6},
      {VK_FORMAT_R16G16B16_SINT, 6},
      {VK_FORMAT_R16G16B16_SFLOAT, 6},
      {VK_FORMAT_R32G32B32_UINT, 12},
      {VK_FORMAT_R32G32B32_SINT, 12},
      {VK_FORMAT_R32G32B32_SFLOAT, 12},
      // 4 channel
      {VK_FORMAT_R8G8B8A8_UNORM, 4},
      {VK_FORMAT_R8G8B8A8_SRGB, 4},
      {VK_FORMAT_R8G8B8A8_SNORM, 4},
      {VK_FORMAT_B8G8R8A8_UNORM, 4},
      {VK_FORMAT_B8G8R8A8_SRGB, 4},
      {VK_FORMAT_R8G8B8A8_UINT, 4},
      {VK_FORMAT_R8G8B8A8_SINT, 4},
      {VK_FORMAT_R16G16B16A16_UNORM, 8},
      {VK_FORMAT_R16G16B16A16_SNORM, 8},
      {VK_FORMAT_R16G16B16A16_UINT, 8},
      {VK_FORMAT_R16G16B16A16_SINT, 8},
      {VK_FORMAT_R16G16B16A16_SFLOAT, 8},
      {VK_FORMAT_R32G32B32A32_UINT, 16},
      {VK_FORMAT_R32G32B32A32_SINT, 16},
      {VK_FORMAT_R32G32B32A32_SFLOAT, 16},
      // Depth / Stencil
      {VK_FORMAT_D16_UNORM, 2},
      {VK_FORMAT_D24_UNORM_S8_UINT, 4},
      {VK_FORMAT_X8_D24_UNORM_PACK32, 4},
      {VK_FORMAT_D32_SFLOAT, 4},
      {VK_FORMAT_D32_SFLOAT_S8_UINT, 5},
      // BC Compressed (block size)
      {VK_FORMAT_BC1_RGB_UNORM_BLOCK, 8},
      {VK_FORMAT_BC1_RGB_SRGB_BLOCK, 8},
      {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8},
      {VK_FORMAT_BC1_RGBA_SRGB_BLOCK, 8},
      {VK_FORMAT_BC2_UNORM_BLOCK, 16},
      {VK_FORMAT_BC2_SRGB_BLOCK, 16},
      {VK_FORMAT_BC3_UNORM_BLOCK, 16},
      {VK_FORMAT_BC3_SRGB_BLOCK, 16},
      {VK_FORMAT_BC4_UNORM_BLOCK, 8},
      {VK_FORMAT_BC4_SNORM_BLOCK, 8},
      {VK_FORMAT_BC5_UNORM_BLOCK, 16},
      {VK_FORMAT_BC5_SNORM_BLOCK, 16},
      {VK_FORMAT_BC6H_UFLOAT_BLOCK, 16},
      {VK_FORMAT_BC6H_SFLOAT_BLOCK, 16},
      {VK_FORMAT_BC7_UNORM_BLOCK, 16},
      {VK_FORMAT_BC7_SRGB_BLOCK, 16},
  };

  auto iter = FormatSizes.find(format);
  if (iter != FormatSizes.end()) {
    return iter->second;
  }

  auto formatStr = ToString(format, 1);
  PrintWarning("Unknown format {} for size retrieval.", formatStr);
  return 0;
}

// NOLINTNEXTLINE
auto StringToImageFormat(const std::string &format) -> VkFormat {
  static const std::unordered_map<std::string, VkFormat> StringToFormat = {
      {"rgba8", VK_FORMAT_R8G8B8A8_UNORM},
      {"rg8", VK_FORMAT_R8G8_UNORM},
      {"r8", VK_FORMAT_R8_UNORM},
      {"rgba16", VK_FORMAT_R16G16B16A16_UNORM},
      {"rg16", VK_FORMAT_R16G16_UNORM},
      {"r16", VK_FORMAT_R16_UNORM},
      {"rgba16f", VK_FORMAT_R16G16B16A16_SFLOAT},
      {"rg16f", VK_FORMAT_R16G16_SFLOAT},
      {"r16f", VK_FORMAT_R16_SFLOAT},
      {"rgba32f", VK_FORMAT_R32G32B32A32_SFLOAT},
      {"rg32f", VK_FORMAT_R32G32_SFLOAT},
      {"r32f", VK_FORMAT_R32_SFLOAT},
      {"rgba8ui", VK_FORMAT_R8G8B8A8_UINT},
      {"rg8ui", VK_FORMAT_R8G8_UINT},
      {"r8ui", VK_FORMAT_R8_UINT},
      {"rgba16ui", VK_FORMAT_R16G16B16A16_UINT},
      {"rg16ui", VK_FORMAT_R16G16_UINT},
      {"r16ui", VK_FORMAT_R16_UINT},
      {"rgba32ui", VK_FORMAT_R32G32B32A32_UINT},
      {"rg32ui", VK_FORMAT_R32G32_UINT},
      {"r32ui", VK_FORMAT_R32_UINT},
      {"rgba8si", VK_FORMAT_R8G8B8A8_SINT},
      {"rg8si", VK_FORMAT_R8G8_SINT},
      {"r8si", VK_FORMAT_R8_SINT},
      {"rgba16si", VK_FORMAT_R16G16B16A16_SINT},
      {"rg16si", VK_FORMAT_R16G16_SINT},
      {"r16si", VK_FORMAT_R16_SINT},
      {"rgba32si", VK_FORMAT_R32G32B32A32_SINT},
      {"rg32si", VK_FORMAT_R32G32_SINT},
      {"r32si", VK_FORMAT_R32_SINT},
      {"depth16", VK_FORMAT_D16_UNORM},
      {"depth24", VK_FORMAT_X8_D24_UNORM_PACK32},
      {"depth32f", VK_FORMAT_D32_SFLOAT},
      {"depth24stencil8", VK_FORMAT_D24_UNORM_S8_UINT},
      {"depth32stencil8", VK_FORMAT_D32_SFLOAT_S8_UINT},
      {"rg11b10f", VK_FORMAT_B10G11R11_UFLOAT_PACK32},
      {"rgb9e5", VK_FORMAT_E5B9G9R9_UFLOAT_PACK32},
      {"rgb10a2", VK_FORMAT_A2B10G10R10_UNORM_PACK32},
      {"rgb10a2ui", VK_FORMAT_A2B10G10R10_UINT_PACK32},
      {"bgr5a1", VK_FORMAT_A1R5G5B5_UNORM_PACK16},
      {"bgr565", VK_FORMAT_R5G6B5_UNORM_PACK16},
      {"rgba4", VK_FORMAT_R4G4B4A4_UNORM_PACK16},
      {"bc1", VK_FORMAT_BC1_RGBA_UNORM_BLOCK},
      {"bc3", VK_FORMAT_BC3_UNORM_BLOCK},
      {"bc4", VK_FORMAT_BC4_UNORM_BLOCK},
      {"bc5", VK_FORMAT_BC5_UNORM_BLOCK},
      {"bc6h", VK_FORMAT_BC6H_UFLOAT_BLOCK},
      {"bc6hs", VK_FORMAT_BC6H_SFLOAT_BLOCK},
      {"bc7", VK_FORMAT_BC7_UNORM_BLOCK},
  };
  auto iter = StringToFormat.find(format);
  if (iter != StringToFormat.end()) {
    return iter->second;
  }

  PrintWarning("Unknown image format string '{}'.", format);

  return VK_FORMAT_UNDEFINED;
}

// NOLINTNEXTLINE
auto ImageFormatToString(VkFormat format) -> std::string_view {
  static const std::unordered_map<VkFormat, std::string> FormatToString = {
      {VK_FORMAT_R8G8B8A8_UNORM, "rgba8"},
      {VK_FORMAT_R8G8B8A8_SRGB, "srgba8"},
      {VK_FORMAT_R16G16B16A16_UNORM, "rgba16"},
      {VK_FORMAT_R16G16B16A16_SFLOAT, "rgba16f"},
      {VK_FORMAT_R32G32B32A32_SFLOAT, "rgba32f"},
      {VK_FORMAT_R8G8B8A8_UINT, "rgba8ui"},
      {VK_FORMAT_R16G16B16A16_UINT, "rgba16ui"},
      {VK_FORMAT_R32G32B32A32_UINT, "rgba32ui"},
      {VK_FORMAT_R8G8B8A8_SINT, "rgba8si"},
      {VK_FORMAT_R16G16B16A16_SINT, "rgba16si"},
      {VK_FORMAT_R32G32B32A32_SINT, "rgba32si"},
      {VK_FORMAT_D16_UNORM, "depth16"},
      {VK_FORMAT_X8_D24_UNORM_PACK32, "depth24"},
      {VK_FORMAT_D32_SFLOAT, "depth32f"},
      {VK_FORMAT_D24_UNORM_S8_UINT, "depth24stencil8"},
      {VK_FORMAT_D32_SFLOAT_S8_UINT, "depth32stencil8"},
      {VK_FORMAT_B10G11R11_UFLOAT_PACK32, "rg11b10f"},
      {VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, "rgb9e5"},
      {VK_FORMAT_A2B10G10R10_UNORM_PACK32, "rgb10a2"},
      {VK_FORMAT_A2B10G10R10_UINT_PACK32, "rgb10a2ui"},
      {VK_FORMAT_A1R5G5B5_UNORM_PACK16, "bgr5a1"},
      {VK_FORMAT_R5G6B5_UNORM_PACK16, "bgr565"},
      {VK_FORMAT_R4G4B4A4_UNORM_PACK16, "rgba4"},
      {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, "bc1"},
      {VK_FORMAT_BC3_UNORM_BLOCK, "bc3"},
      {VK_FORMAT_BC4_UNORM_BLOCK, "bc4"},
      {VK_FORMAT_BC5_UNORM_BLOCK, "bc5"},
      {VK_FORMAT_BC6H_UFLOAT_BLOCK, "bc6h"},
      {VK_FORMAT_BC6H_SFLOAT_BLOCK, "bc6hs"},
      {VK_FORMAT_BC7_UNORM_BLOCK, "bc7"},
  };
  auto iter = FormatToString.find(format);
  if (iter != FormatToString.end()) {
    return iter->second;
  }
  return "undefined";
}

auto ToString(VkFormat format, size_t arraySize) -> std::string_view {
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
auto FromString(const std::string &format) -> VkFormat {
  static const std::unordered_map<std::string, VkFormat> StringToFormat = {
      {"unknown", VK_FORMAT_UNDEFINED},
      {"float", VK_FORMAT_R32_SFLOAT},
      {"floatvec2", VK_FORMAT_R32G32_SFLOAT},
      {"floatvec3", VK_FORMAT_R32G32B32_SFLOAT},
      {"floatvec4", VK_FORMAT_R32G32B32A32_SFLOAT},
      {"half", VK_FORMAT_R16_SFLOAT},
      {"halfvec2", VK_FORMAT_R16G16_SFLOAT},
      {"halfvec3", VK_FORMAT_R16G16B16_SFLOAT},
      {"halfvec4", VK_FORMAT_R16G16B16A16_SFLOAT},
      {"uint8", VK_FORMAT_R8_UINT},
      {"uint8vec2", VK_FORMAT_R8G8_UINT},
      {"uint8vec3", VK_FORMAT_R8G8B8_UINT},
      {"uint8vec4", VK_FORMAT_R8G8B8A8_UINT},
      {"uint16", VK_FORMAT_R16_UINT},
      {"uint16vec2", VK_FORMAT_R16G16_UINT},
      {"uint16vec3", VK_FORMAT_R16G16B16_UINT},
      {"uint16vec4", VK_FORMAT_R16G16B16A16_UINT},
      {"uint32", VK_FORMAT_R32_UINT},
      {"uint32vec2", VK_FORMAT_R32G32_UINT},
      {"uint32vec3", VK_FORMAT_R32G32B32_UINT},
      {"uint32vec4", VK_FORMAT_R32G32B32A32_UINT},
      {"int8", VK_FORMAT_R8_SINT},
      {"int8vec2", VK_FORMAT_R8G8_SINT},
      {"int8vec3", VK_FORMAT_R8G8B8_SINT},
      {"int8vec4", VK_FORMAT_R8G8B8A8_SINT},
      {"int16", VK_FORMAT_R16_SINT},
      {"int16vec2", VK_FORMAT_R16G16_SINT},
      {"int16vec3", VK_FORMAT_R16G16B16_SINT},
      {"int16vec4", VK_FORMAT_R16G16B16A16_SINT},
      {"int32", VK_FORMAT_R32_SINT},
      {"int32vec2", VK_FORMAT_R32G32_SINT},
      {"int32vec3", VK_FORMAT_R32G32B32_SINT},
      {"int32vec4", VK_FORMAT_R32G32B32A32_SINT},
      {"unorm8", VK_FORMAT_R8_UNORM},
      {"unorm8vec2", VK_FORMAT_R8G8_UNORM},
      {"unorm8vec3", VK_FORMAT_R8G8B8_UNORM},
      {"unorm8vec4", VK_FORMAT_R8G8B8A8_UNORM},
      {"unorm16", VK_FORMAT_R16_UNORM},
      {"unorm16vec2", VK_FORMAT_R16G16_UNORM},
      {"unorm16vec3", VK_FORMAT_R16G16B16_UNORM},
      {"unorm16vec4", VK_FORMAT_R16G16B16A16_UNORM},
      {"snorm8", VK_FORMAT_R8_SNORM},
      {"snorm8vec2", VK_FORMAT_R8G8_SNORM},
      {"snorm8vec3", VK_FORMAT_R8G8B8_SNORM},
      {"snorm8vec4", VK_FORMAT_R8G8B8A8_SNORM},
      {"snorm16", VK_FORMAT_R16_SNORM},
      {"snorm16vec2", VK_FORMAT_R16G16_SNORM},
      {"snorm16vec3", VK_FORMAT_R16G16B16_SNORM},
      {"snorm16vec4", VK_FORMAT_R16G16B16A16_SNORM},
      {"floatmat2", VK_FORMAT_R32G32_SFLOAT},
      {"floatmat2x2", VK_FORMAT_R32G32_SFLOAT},
      {"floatmat3", VK_FORMAT_R32G32B32_SFLOAT},
      {"floatmat3x3", VK_FORMAT_R32G32B32_SFLOAT},
      {"floatmat4", VK_FORMAT_R32G32B32A32_SFLOAT},
      {"floatmat4x4", VK_FORMAT_R32G32B32A32_SFLOAT},
      {"halfmat2", VK_FORMAT_R16G16_SFLOAT},
      {"halfmat2x2", VK_FORMAT_R16G16_SFLOAT},
      {"halfmat3", VK_FORMAT_R16G16B16_SFLOAT},
      {"halfmat3x3", VK_FORMAT_R16G16B16_SFLOAT},
      {"halfmat4", VK_FORMAT_R16G16B16A16_SFLOAT},
      {"halfmat4x4", VK_FORMAT_R16G16B16A16_SFLOAT},
      {"floatmat2x3", VK_FORMAT_R32G32B32_SFLOAT},
      {"floatmat3x2", VK_FORMAT_R32G32_SFLOAT},
      {"floatmat2x4", VK_FORMAT_R32G32B32A32_SFLOAT},
      {"floatmat4x2", VK_FORMAT_R32G32_SFLOAT},
      {"floatmat3x4", VK_FORMAT_R32G32B32A32_SFLOAT},
      {"floatmat4x3", VK_FORMAT_R32G32B32_SFLOAT},
      {"halfmat2x3", VK_FORMAT_R16G16B16_SFLOAT},
      {"halfmat3x2", VK_FORMAT_R16G16_SFLOAT},
      {"halfmat2x4", VK_FORMAT_R16G16B16A16_SFLOAT},
      {"halfmat4x2", VK_FORMAT_R16G16_SFLOAT},
      {"halfmat3x4", VK_FORMAT_R16G16B16A16_SFLOAT},
      {"halfmat4x3", VK_FORMAT_R16G16B16_SFLOAT},
  };
  auto iter = StringToFormat.find(format);
  if (iter != StringToFormat.end()) {
    return iter->second;
  }
  PrintWarning("Unknown format string: " + format);
  return VK_FORMAT_UNDEFINED;
}

auto StringToArraySize(const std::string &format) -> size_t {
  static const std::unordered_map<std::string, size_t> FormatToArraySize = {
      {"floatmat2", 2},   {"floatmat2x2", 2}, {"floatmat3", 3},
      {"floatmat3x3", 3}, {"floatmat4", 4},   {"floatmat4x4", 4},
      {"halfmat2", 2},    {"halfmat2x2", 2},  {"halfmat3", 3},
      {"halfmat3x3", 3},  {"halfmat4", 4},    {"halfmat4x4", 4},
      {"floatmat2x3", 3}, {"floatmat3x2", 2}, {"floatmat2x4", 4},
      {"floatmat4x2", 2}, {"floatmat3x4", 4}, {"floatmat4x3", 3},
      {"halfmat2x3", 3},  {"halfmat3x2", 2},  {"halfmat2x4", 4},
      {"halfmat4x2", 2},  {"halfmat3x4", 4},  {"halfmat4x3", 3},
  };
  auto iter = FormatToArraySize.find(format);
  if (iter != FormatToArraySize.end()) {
    return iter->second;
  }

  return 1; // default array size is 1 for non-matrix types
}

auto GetVec4Variant(VkFormat format) -> VkFormat {
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

auto GetBaseFormat(VkFormat format) -> VkFormat {
  switch (format) {
  case VK_FORMAT_R32G32B32A32_SFLOAT:
  case VK_FORMAT_R32G32B32_SFLOAT:
  case VK_FORMAT_R32G32_SFLOAT:
  case VK_FORMAT_R32_SFLOAT:
    return VK_FORMAT_R32_SFLOAT;

  case VK_FORMAT_R16G16B16A16_SFLOAT:
  case VK_FORMAT_R16G16B16_SFLOAT:
  case VK_FORMAT_R16G16_SFLOAT:
  case VK_FORMAT_R16_SFLOAT:
    return VK_FORMAT_R16_SFLOAT;

  case VK_FORMAT_R8G8B8A8_UINT:
  case VK_FORMAT_R8G8B8_UINT:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8_UINT:
    return VK_FORMAT_R8_UINT;

  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16_UINT:
    return VK_FORMAT_R16_UINT;

  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32_UINT:
    return VK_FORMAT_R32_UINT;

  case VK_FORMAT_R8G8B8A8_SINT:
  case VK_FORMAT_R8G8B8_SINT:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8_SINT:
    return VK_FORMAT_R8_SINT;

  case VK_FORMAT_R16G16B16A16_SINT:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16_SINT:
    return VK_FORMAT_R16_SINT;

  case VK_FORMAT_R32G32B32A32_SINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32_SINT:
    return VK_FORMAT_R32_SINT;

  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8_UNORM:
    return VK_FORMAT_R8_UNORM;

  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16_UNORM:
    return VK_FORMAT_R16_UNORM;

  case VK_FORMAT_R8G8B8A8_SNORM:
  case VK_FORMAT_R8G8B8_SNORM:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8_SNORM:
    return VK_FORMAT_R8_SNORM;

  case VK_FORMAT_R16G16B16A16_SNORM:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16_SNORM:
    return VK_FORMAT_R16_SNORM;

  default:
    return format;
  }
}

auto BaseTypeToString(VkFormat format, uint8_t const *data) -> std::string {
  assert(data != nullptr);

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-reinterpret-cast)

  constexpr auto UINT_NORM_MAX = 255.0F;
  constexpr auto USHORT_NORM_MAX = 65535.0F;
  constexpr auto SINT_NORM_MAX = 127.0F;
  constexpr auto SINT_NORM_MIN = -128.0F;
  constexpr auto SHORT_NORM_MAX = 32767.0F;
  constexpr auto SHORT_NORM_MIN = -32768.0F;

  switch (format) {
  case VK_FORMAT_R32_SFLOAT:
    return std::to_string(*reinterpret_cast<float const *>(data));
  case VK_FORMAT_R16_SFLOAT: {
    auto value = *reinterpret_cast<numeric::float16_t const *>(data);
    return std::to_string(static_cast<float>(value));
  }
  case VK_FORMAT_R8_UINT:
    return std::to_string(*data);
  case VK_FORMAT_R16_UINT:
    return std::to_string(*reinterpret_cast<uint16_t const *>(data));
  case VK_FORMAT_R32_UINT:
    return std::to_string(*reinterpret_cast<uint32_t const *>(data));
  case VK_FORMAT_R8_SINT:
    return std::to_string(*reinterpret_cast<int8_t const *>(data));
  case VK_FORMAT_R16_SINT:
    return std::to_string(*reinterpret_cast<int16_t const *>(data));
  case VK_FORMAT_R32_SINT:
    return std::to_string(*reinterpret_cast<int32_t const *>(data));
  case VK_FORMAT_R8_UNORM:
    return std::to_string(static_cast<float>(*data) / UINT_NORM_MAX);
  case VK_FORMAT_R16_UNORM:
    return std::to_string(
        static_cast<float>(*reinterpret_cast<uint16_t const *>(data)) /
        USHORT_NORM_MAX);
  case VK_FORMAT_R8_SNORM:
    return std::to_string(
        std::max(static_cast<float>(*reinterpret_cast<int8_t const *>(data)) /
                     SINT_NORM_MAX,
                 SINT_NORM_MIN));
  case VK_FORMAT_R16_SNORM:
    return std::to_string(
        std::max(static_cast<float>(*reinterpret_cast<int16_t const *>(data)) /
                     SHORT_NORM_MAX,
                 SHORT_NORM_MIN));
  default:
    return "unknown";
  }

  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-reinterpret-cast)
}

auto ToString(VkFormat format, uint8_t const *data) -> std::string {
  assert(data != nullptr);
  auto baseFormat = GetBaseFormat(format);
  if (baseFormat == format) {
    return BaseTypeToString(format, data);
  }

  auto channelCount = GetChannelCount(format);
  auto offset = 0U;
  auto stride = GetSize(baseFormat);
  std::string values;

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  for (size_t i = 0; i < channelCount; ++i) {
    auto channelStr = BaseTypeToString(baseFormat, data + offset);
    offset += stride;
    values += channelStr;

    if (i < channelCount - 1) {
      values += ", ";
    }
  }

  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  return values;
}

} // namespace Graphics::Format