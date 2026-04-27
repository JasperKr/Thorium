#pragma once
#include <cstddef>

#include <string>
#include <vulkan/vulkan_core.h>
namespace Graphics::Format {
auto GetChannelCount(VkFormat format) -> uint32_t;
auto GetSize(VkFormat format) -> uint32_t;
auto StringToImageFormat(const std::string &format) -> VkFormat;
auto ImageFormatToString(VkFormat format) -> std::string;
auto ToString(VkFormat format, size_t arraySize = 1) -> std::string;
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