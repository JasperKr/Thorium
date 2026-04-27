// Graphics snapshot module
// Can be used to see what happened during a frame in-engine to help with profiling and debugging.

#pragma once

#include "Graphics/bufferformat.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Graphics::Snapshot {

enum class EventType : uint8_t {
  Unknown,

  Create_Buffer,
  Create_Texture,
  Create_Pipeline,
  Create_DescriptorSet,
  Create_Sampler,
  Create_ShaderModule,

  Destroy_Buffer,
  Destroy_Texture,
  Destroy_Pipeline,
  Destroy_DescriptorSet,
  Destroy_Sampler,
  Destroy_ShaderModule,

  Structured_Buffer_Upload,

  Upload_To_Buffer,
  Upload_To_Texture,

  Copy_Buffer_To_Buffer,
  Copy_Buffer_To_Texture,
  Copy_Texture_To_Buffer,
  Copy_Texture_To_Texture,

  Draw,
  DrawIndexed,
  DrawIndirect,
  DrawIndexedIndirect,

  Dispatch,
  DispatchIndirect,

  Set_Pipeline,
  Set_DescriptorSet,
  Set_VertexBuffer,
  Set_IndexBuffer,
  Set_PushConstants,
};

auto EventTypeToString(EventType type) -> const char *;
auto EventTypeFromString(const std::string &str) -> EventType;

using Handle = uint64_t;
using Timestamp = uint64_t;

struct Event {
  Event(const Event &) = default;
  Event(Event &&) = delete;
  auto operator=(const Event &) -> Event & = default;
  auto operator=(Event &&) -> Event & = delete;
  Event(EventType type, Timestamp timestamp)
      : type(type), timestamp(timestamp) {}
  virtual ~Event();

  EventType type;
  Timestamp timestamp;

  auto DrawImGui() const -> void;
  virtual auto DrawVariantImGui() const -> void;
};

struct GraphicsEvent {
  int renderState;

  GraphicsEvent();

  auto DrawStateImGui() const -> void;
};

struct BufferCreateEvent : public Event {
  Handle bufferHandle;
  Handle memoryHandle;
  VkDeviceSize size;
  uint32_t usage;
  uint32_t properties;

  auto DrawVariantImGui() const -> void override;
};

struct BufferDestroyEvent : public Event {
  Handle bufferHandle;
  Handle memoryHandle;

  auto DrawVariantImGui() const -> void override;
};

struct BufferUploadEvent : public Event {
  Handle bufferHandle;
  Handle memoryHandle;
  VkDeviceSize offset;
  VkDeviceSize size;

  std::vector<uint8_t> data;

  auto DrawVariantImGui() const -> void override;
};

struct StructuredBufferUploadEvent : public Event {
  // If this event happens, it means we will be uploading to a structured buffer
  // Next event MUST be a BufferUploadEvent with the same bufferHandle.

  Handle bufferHandle;
  BufferFormat format;

  BufferUploadEvent *uploadEvent;

  auto DrawVariantImGui() const -> void override;
};

struct BufferCopyEvent : public Event {
  Handle srcBufferHandle;
  Handle dstBufferHandle;
  VkDeviceSize srcOffset;
  VkDeviceSize dstOffset;
  VkDeviceSize size;

  auto DrawVariantImGui() const -> void override;
};

struct TextureCreateEvent : public Event {
  Handle textureHandle;
  Handle memoryHandle;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t mipLevels;
  uint32_t arrayLayers;
  uint32_t format;
  uint32_t usage;
  uint32_t properties;

  auto DrawVariantImGui() const -> void override;
};

struct TextureDestroyEvent : public Event {
  Handle textureHandle;
  Handle memoryHandle;

  auto DrawVariantImGui() const -> void override;
};

struct TextureUploadEvent : public Event {
  Handle textureHandle;
  Handle memoryHandle;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t mipLevel;
  uint32_t arrayLayer;
  uint32_t format;
  VkDeviceSize dataSize;

  auto DrawVariantImGui() const -> void override;
};

struct TextureCopyEvent : public Event {
  Handle srcTextureHandle;
  Handle dstTextureHandle;
  uint32_t srcWidth;
  uint32_t srcHeight;
  uint32_t srcDepth;
  uint32_t srcMipLevel;
  uint32_t srcArrayLayer;
  uint32_t dstWidth;
  uint32_t dstHeight;
  uint32_t dstDepth;
  uint32_t dstMipLevel;
  uint32_t dstArrayLayer;
  VkDeviceSize dataSize;

  auto DrawVariantImGui() const -> void override;
};

struct PipelineCreateEvent : public Event {
  Handle pipelineHandle;
  uint32_t pipelineType;

  auto DrawVariantImGui() const -> void override;
};

struct PipelineDestroyEvent : public Event {
  Handle pipelineHandle;

  auto DrawVariantImGui() const -> void override;
};

struct DescriptorSetCreateEvent : public Event {
  Handle descriptorSetHandle;

  auto DrawVariantImGui() const -> void override;
};

struct DescriptorSetDestroyEvent : public Event {
  Handle descriptorSetHandle;

  auto DrawVariantImGui() const -> void override;
};

struct SamplerCreateEvent : public Event {
  Handle samplerHandle;

  VkFilter magFilter;
  VkFilter minFilter;
  VkSamplerMipmapMode mipmapMode;
  VkSamplerAddressMode addressModeU;
  VkSamplerAddressMode addressModeV;
  VkSamplerAddressMode addressModeW;
  float mipLodBias;
  bool anisotropyEnable;
  float maxAnisotropy;
  bool compareEnable;
  VkCompareOp compareOp;
  float minLod;
  float maxLod;
  VkBorderColor borderColor;

  auto DrawVariantImGui() const -> void override;
};

struct SamplerDestroyEvent : public Event {
  Handle samplerHandle;

  auto DrawVariantImGui() const -> void override;
};

struct ShaderModuleCreateEvent : public Event {
  Handle shaderModuleHandle;
  uint32_t codeSize;
  std::string moduleName;

  auto DrawVariantImGui() const -> void override;
};

struct ShaderModuleDestroyEvent : public Event {
  Handle shaderModuleHandle;

  auto DrawVariantImGui() const -> void override;
};

struct DrawEvent : public Event, public GraphicsEvent {
  uint32_t vertexCount;
  uint32_t instanceCount;
  uint32_t firstVertex;
  uint32_t firstInstance;

  auto DrawVariantImGui() const -> void override;
};

struct DrawIndexedEvent : public Event, public GraphicsEvent {
  uint32_t indexCount;
  uint32_t instanceCount;
  uint32_t firstIndex;
  int32_t vertexOffset;
  uint32_t firstInstance;

  auto DrawVariantImGui() const -> void override;
};

struct DrawIndirectEvent : public Event, public GraphicsEvent {
  Handle indirectBufferHandle;
  VkDeviceSize offset;
  uint32_t drawCount;
  uint32_t stride;

  auto DrawVariantImGui() const -> void override;
};

struct DrawIndexedIndirectEvent : public Event, public GraphicsEvent {
  Handle indirectBufferHandle;
  VkDeviceSize offset;
  uint32_t drawCount;
  uint32_t stride;

  auto DrawVariantImGui() const -> void override;
};

struct DispatchEvent : public Event, public GraphicsEvent {
  uint32_t groupCountX;
  uint32_t groupCountY;
  uint32_t groupCountZ;

  auto DrawVariantImGui() const -> void override;
};

struct DispatchIndirectEvent : public Event, public GraphicsEvent {
  Handle indirectBufferHandle;
  VkDeviceSize offset;

  auto DrawVariantImGui() const -> void override;
};

struct SetPipelineEvent : public Event, public GraphicsEvent {
  Handle pipelineHandle;
  uint32_t pipelineBindPoint;

  auto DrawVariantImGui() const -> void override;
};

struct SetDescriptorSetEvent : public Event {
  Handle descriptorSetHandle;
  uint32_t setIndex;

  auto DrawVariantImGui() const -> void override;
};

struct SetVertexBufferEvent : public Event {
  Handle bufferHandle;
  VkDeviceSize offset;

  auto DrawVariantImGui() const -> void override;
};

struct SetIndexBufferEvent : public Event {
  Handle bufferHandle;
  VkDeviceSize offset;
  uint32_t indexType;

  auto DrawVariantImGui() const -> void override;
};

struct SetPushConstantsEvent : public Event {
  uint32_t layoutHandle;
  uint32_t stageFlags;
  uint32_t offset;
  uint32_t size;

  std::vector<uint8_t> data;
  BufferFormat pushConstantFormat;

  auto DrawVariantImGui() const -> void override;
};

static const Type ThreadSnapshotType = Type("ThreadSnapshot");

struct ThreadSnapshot : Object {
  std::vector<Event> events;
  std::vector<DynamicRendering::State> renderStates;

  uint64_t threadId;
  std::string threadName;

  [[nodiscard]] auto GetName() const -> std::string {
    if (!threadName.empty()) {
      return threadName;
    }

    return "Thread " + std::to_string(threadId);
  }

  static auto GetType() -> Type const * { return &ThreadSnapshotType; }
  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return GetType();
  }
};

auto GetCurrentSnapshot() -> ThreadSnapshot *;
auto CaptureEvent(const Event &event) -> Error;
auto Save(const ThreadSnapshot &snapshot, const std::string &filename) -> void;
auto Load(const std::string &filename) -> ThreadSnapshot;
auto StartSnapshot() -> void;
auto EndSnapshot() -> void;
auto RenderSnapshot(const ThreadSnapshot &snapshot) -> void;

} // namespace Graphics::Snapshot