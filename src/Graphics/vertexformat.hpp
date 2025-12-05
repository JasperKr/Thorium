#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

namespace Graphics {

struct VertexFormat {
  std::vector<VkVertexInputAttributeDescription> Attributes;
  std::vector<VkVertexInputBindingDescription> Bindings;
};

enum class VertexFormats : uint8_t {
  Unknown = 0,
  Default,
  Animated,
  DefaultInstanced,
  AnimatedInstanced,
  ImGui,
  Default2D
};

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays, hicpp-avoid-c-arrays)

struct Format_Default {
  float position[3];
  uint32_t normal;  // a2b10g10r10
  uint32_t tangent; // a2b10g10r10
  uint16_t uv[4];   // 2x half
  uint16_t uv2[4];  // 2x half
};

struct Format_Animated {
  float position[3];
  uint32_t normal;  // a2b10g10r10
  uint32_t tangent; // a2b10g10r10
  uint16_t uv[4];   // 2x half
  uint16_t uv2[4];  // 2x half
  float boneWeights[4];
  uint32_t boneIndices[4];
};

struct Format_ImGui {
  float position[2];
  float uv[2];
  uint32_t color; // RGBA8
};

struct Format_Default2D {
  float position[2];
  float uv[2];
  uint32_t color; // RGBA8
};

// NOLINTEND(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays, hicpp-avoid-c-arrays)

struct VertexFormatsHash {
  auto operator()(VertexFormats format) const noexcept -> size_t {
    return static_cast<size_t>(format);
  }
};

const static std::unordered_map<const VertexFormats, const VertexFormat,
                                VertexFormatsHash>
    PredefinedVertexFormats = {
        {VertexFormats::Default,
         {
             .Attributes = {{// Vec3 Position
                             .location = 0,
                             .binding = 0,
                             .format = VK_FORMAT_R32G32B32_SFLOAT,
                             .offset = 0},
                            {// a2b10g10r10 Normal
                             .location = 1,
                             .binding = 0,
                             .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                             .offset = 12},
                            {
                                // a2b10g10r10 Tangent
                                .location = 2,
                                .binding = 0,
                                .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                                .offset = 16,
                            },
                            {// 2x half UV
                             .location = 3,
                             .binding = 0,
                             .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                             .offset = 20},
                            {// 2x half UV 2
                             .location = 4,
                             .binding = 0,
                             .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                             .offset = 28}},
             .Bindings = {{.binding = 0,
                           .stride = 36,
                           .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}},
         }},
        {VertexFormats::Animated,
         {
             .Attributes = {{// Vec3 Position
                             .location = 0,
                             .binding = 0,
                             .format = VK_FORMAT_R32G32B32_SFLOAT,
                             .offset = 0},
                            {// a2b10g10r10 Normal
                             .location = 1,
                             .binding = 0,
                             .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                             .offset = 12},
                            {
                                // a2b10g10r10 Tangent
                                .location = 2,
                                .binding = 0,
                                .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                                .offset = 16,
                            },
                            {// 2x half UV
                             .location = 3,
                             .binding = 0,
                             .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                             .offset = 20},
                            {// 2x half UV 2
                             .location = 4,
                             .binding = 0,
                             .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                             .offset = 28},
                            {// vec4 Bone Weights
                             .location = 5,
                             .binding = 0,
                             .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                             .offset = 36},
                            {// uvec4 Bone Indices
                             .location = 6,
                             .binding = 0,
                             .format = VK_FORMAT_R32G32B32A32_UINT,
                             .offset = 52}},
             .Bindings = {{.binding = 0,
                           .stride = 68,
                           .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}},
         }},
        {VertexFormats::DefaultInstanced,
         {
             .Attributes =
                 {
                     {// Vec3 Position
                      .location = 0,
                      .binding = 0,
                      .format = VK_FORMAT_R32G32B32_SFLOAT,
                      .offset = 0},
                     {// a2b10g10r10 Normal
                      .location = 1,
                      .binding = 0,
                      .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                      .offset = 12},
                     {
                         // a2b10g10r10 Tangent
                         .location = 2,
                         .binding = 0,
                         .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                         .offset = 16,
                     },
                     {// 2x half UV
                      .location = 3,
                      .binding = 0,
                      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                      .offset = 20},
                     {// 2x half UV 2
                      .location = 4,
                      .binding = 0,
                      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                      .offset = 28},
                     {// mat4 Instance Transform (location 5,6,7,8)
                      .location = 5,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 0},
                     {.location = 6,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 16},
                     {.location = 7,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 32},
                     {.location = 8,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 48},
                 },
             .Bindings = {{.binding = 0,
                           .stride = 36,
                           .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
                          {.binding = 1,
                           .stride = 64,
                           .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE}},
         }},
        {VertexFormats::AnimatedInstanced,
         {
             .Attributes =
                 {
                     {// Vec3 Position
                      .location = 0,
                      .binding = 0,
                      .format = VK_FORMAT_R32G32B32_SFLOAT,
                      .offset = 0},
                     {// a2b10g10r10 Normal
                      .location = 1,
                      .binding = 0,
                      .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                      .offset = 12},
                     {
                         // a2b10g10r10 Tangent
                         .location = 2,
                         .binding = 0,
                         .format = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                         .offset = 16,
                     },
                     {// 2x half UV
                      .location = 3,
                      .binding = 0,
                      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                      .offset = 20},
                     {// 2x half UV 2
                      .location = 4,
                      .binding = 0,
                      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                      .offset = 28},
                     {// vec4 Bone Weights
                      .location = 5,
                      .binding = 0,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 36},
                     {// uvec4 Bone Indices
                      .location = 6,
                      .binding = 0,
                      .format = VK_FORMAT_R32G32B32A32_UINT,
                      .offset = 52},
                     {// mat4 Instance Transform (location 7,8,9,10)
                      .location = 7,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 0},
                     {.location = 8,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 16},
                     {.location = 9,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 32},
                     {.location = 10,
                      .binding = 1,
                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .offset = 48},
                 },
             .Bindings = {{.binding = 0,
                           .stride = 68,
                           .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
                          {.binding = 1,
                           .stride = 64,
                           .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE}},
         }},
        {VertexFormats::ImGui,
         {.Attributes = {{// Vec2 Position
                          .location = 0,
                          .binding = 0,
                          .format = VK_FORMAT_R32G32_SFLOAT,
                          .offset = 0},
                         {// Vec2 UV
                          .location = 1,
                          .binding = 0,
                          .format = VK_FORMAT_R32G32_SFLOAT,
                          .offset = 8},
                         {// unorm8 Color
                          .location = 2,
                          .binding = 0,
                          .format = VK_FORMAT_R8G8B8A8_UNORM,
                          .offset = 16}},
          .Bindings = {{.binding = 0,
                        .stride = 20,
                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}}}},
        {VertexFormats::Default2D,
         {.Attributes = {{// Vec2 Position
                          .location = 0,
                          .binding = 0,
                          .format = VK_FORMAT_R32G32_SFLOAT,
                          .offset = 0},
                         {// Vec2 UV
                          .location = 1,
                          .binding = 0,
                          .format = VK_FORMAT_R32G32_SFLOAT,
                          .offset = 8},
                         {// unorm8 Color
                          .location = 2,
                          .binding = 0,
                          .format = VK_FORMAT_R8G8B8A8_UNORM,
                          .offset = 16}},
          .Bindings = {{.binding = 0,
                        .stride = 20,
                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}}}}};
} // namespace Graphics