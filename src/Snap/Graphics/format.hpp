#pragma once
#include <cstddef>

#include <string>
#include <vulkan/vulkan_core.h>
namespace Graphics::Format {
auto GetChannelCount(VkFormat format) -> uint32_t;
auto GetSize(VkFormat format) -> uint32_t;
auto GetSize(VkFormat format, uint32_t width, uint32_t height) -> uint64_t;
auto IsCompressedFormat(VkFormat format) -> bool;
auto StringToImageFormat(const std::string &format) -> VkFormat;
auto ImageFormatToString(VkFormat format) -> std::string_view;
auto ToString(VkFormat format, size_t arraySize = 1) -> std::string_view;
auto FromString(const std::string &format) -> VkFormat;
auto StringToArraySize(const std::string &format) -> size_t;
auto GetVec4Variant(VkFormat format) -> VkFormat;
auto GetBaseFormat(VkFormat format) -> VkFormat;

// Only allows base types, not vector or matrix types
auto BaseTypeToString(VkFormat format, uint8_t const *data) -> std::string;

// Allows base and vector types, but not matrix types.
// For matrix types you should just call this n times.
auto ToString(VkFormat format, uint8_t const *data) -> std::string;

} // namespace Graphics::Format