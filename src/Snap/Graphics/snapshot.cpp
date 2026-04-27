#include "snapshot.hpp"
#include "Graphics/blendmode.hpp"
#include "Graphics/format.hpp"
#include "Modules/error.hpp"
#include "Modules/utils.hpp"
#include <imgui.h>
#include <string>
#include <variant>
#include <vulkan/vulkan_core.h>

namespace Graphics::Snapshot {

auto EventTypeToString(EventType type) -> const char * {
  switch (type) {
  case EventType::Create_Buffer:
    return "Create_Buffer";
  case EventType::Create_Texture:
    return "Create_Texture";
  case EventType::Create_Pipeline:
    return "Create_Pipeline";
  case EventType::Create_DescriptorSet:
    return "Create_DescriptorSet";
  case EventType::Create_Sampler:
    return "Create_Sampler";
  case EventType::Create_ShaderModule:
    return "Create_ShaderModule";
  case EventType::Destroy_Buffer:
    return "Destroy_Buffer";
  case EventType::Destroy_Texture:
    return "Destroy_Texture";
  case EventType::Destroy_Pipeline:
    return "Destroy_Pipeline";
  case EventType::Destroy_DescriptorSet:
    return "Destroy_DescriptorSet";
  case EventType::Destroy_Sampler:
    return "Destroy_Sampler";
  case EventType::Destroy_ShaderModule:
    return "Destroy_ShaderModule";
  case EventType::Structured_Buffer_Upload:
    return "Structured_Buffer_Upload";
  case EventType::Upload_To_Buffer:
    return "Upload_To_Buffer";
  case EventType::Upload_To_Texture:
    return "Upload_To_Texture";
  case EventType::Copy_Buffer_To_Buffer:
    return "Copy_Buffer_To_Buffer";
  case EventType::Copy_Buffer_To_Texture:
    return "Copy_Buffer_To_Texture";
  case EventType::Copy_Texture_To_Buffer:
    return "Copy_Texture_To_Buffer";
  case EventType::Copy_Texture_To_Texture:
    return "Copy_Texture_To_Texture";
  case EventType::Draw:
    return "Draw";
  case EventType::DrawIndexed:
    return "DrawIndexed";
  case EventType::DrawIndirect:
    return "DrawIndirect";
  case EventType::DrawIndexedIndirect:
    return "DrawIndexedIndirect";
  case EventType::Dispatch:
    return "Dispatch";
  case EventType::DispatchIndirect:
    return "DispatchIndirect";
  case EventType::Set_Pipeline:
    return "Set_Pipeline";
  case EventType::Set_DescriptorSet:
    return "Set_DescriptorSet";
  case EventType::Set_VertexBuffer:
    return "Set_VertexBuffer";
  case EventType::Set_IndexBuffer:
    return "Set_IndexBuffer";
  case EventType::Set_PushConstants:
    return "Set_PushConstants";
  default:
    return "Unknown";
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto EventTypeFromString(const std::string &str) -> EventType {
  // clang-format off
  if (str == "Create_Buffer") { return EventType::Create_Buffer; }
  if (str == "Create_Texture") { return EventType::Create_Texture; }
  if (str == "Create_Pipeline") { return EventType::Create_Pipeline; }
  if (str == "Create_DescriptorSet") { return EventType::Create_DescriptorSet; }
  if (str == "Create_Sampler") { return EventType::Create_Sampler; }
  if (str == "Create_ShaderModule") { return EventType::Create_ShaderModule; }
  if (str == "Destroy_Buffer") { return EventType::Destroy_Buffer; }
  if (str == "Destroy_Texture") { return EventType::Destroy_Texture; }
  if (str == "Destroy_Pipeline") { return EventType::Destroy_Pipeline; }
  if (str == "Destroy_DescriptorSet") { return EventType::Destroy_DescriptorSet; }
  if (str == "Destroy_Sampler") { return EventType::Destroy_Sampler; }
  if (str == "Destroy_ShaderModule") { return EventType::Destroy_ShaderModule; }
  if (str == "Structured_Buffer_Upload") { return EventType::Structured_Buffer_Upload; }
  if (str == "Upload_To_Buffer") { return EventType::Upload_To_Buffer; }
  if (str == "Upload_To_Texture") { return EventType::Upload_To_Texture; }
  if (str == "Copy_Buffer_To_Buffer") { return EventType::Copy_Buffer_To_Buffer; }
  if (str == "Copy_Buffer_To_Texture") { return EventType::Copy_Buffer_To_Texture; }
  if (str == "Copy_Texture_To_Buffer") { return EventType::Copy_Texture_To_Buffer; }
  if (str == "Copy_Texture_To_Texture") { return EventType::Copy_Texture_To_Texture; }
  if (str == "Draw") { return EventType::Draw; }
  if (str == "DrawIndexed") { return EventType::DrawIndexed; }
  if (str == "DrawIndirect") { return EventType::DrawIndirect; }
  if (str == "DrawIndexedIndirect") { return EventType::DrawIndexedIndirect; }
  if (str == "Dispatch") { return EventType::Dispatch; }
  if (str == "DispatchIndirect") { return EventType::DispatchIndirect; }
  if (str == "Set_Pipeline") { return EventType::Set_Pipeline; }
  if (str == "Set_DescriptorSet") { return EventType::Set_DescriptorSet; }
  if (str == "Set_VertexBuffer") { return EventType::Set_VertexBuffer; }
  if (str == "Set_IndexBuffer") { return EventType::Set_IndexBuffer; }
  if (str == "Set_PushConstants") { return EventType::Set_PushConstants; }
  return EventType::Unknown;
  // clang-format on
}

auto GetCurrentSnapshot() -> ThreadSnapshot * {
  thread_local ThreadSnapshot currentSnapshot;
  return &currentSnapshot;
}

auto CaptureEvent(const Event &event) -> Error {
  auto *currentSnapshot = GetCurrentSnapshot();

  if (currentSnapshot == nullptr) {
    return Error::Create("No current snapshot available for this thread.");
  }

  currentSnapshot->events.emplace_back(event);

  auto index = currentSnapshot->events.size() - 1;
  auto *newEvent = &currentSnapshot->events[index];

  if (index > 0) {
    auto *back = &currentSnapshot->events[index - 1];

    if (back->type == EventType::Structured_Buffer_Upload &&
        newEvent->type == EventType::Upload_To_Buffer) {

      // NOLINTBEGIN(cppcoreguidelines-pro-type-static-cast-downcast)
      auto *structuredEvent = static_cast<StructuredBufferUploadEvent *>(back);
      auto *bufferUploadEvent = static_cast<BufferUploadEvent *>(newEvent);
      // NOLINTEND(cppcoreguidelines-pro-type-static-cast-downcast)

      structuredEvent->uploadEvent = bufferUploadEvent;
    }
  }

  return {};
}

auto StartSnapshot() -> void {
  auto *currentSnapshot = GetCurrentSnapshot();
  currentSnapshot->events.clear();
  currentSnapshot->renderStates.clear();
}
auto EndSnapshot() -> void {}
auto RenderSnapshot(const ThreadSnapshot &snapshot) -> void {
  if (ImGui::Begin("Snapshot Viewer")) {
    for (const auto &event : snapshot.events) {
      ImGui::Text("%s", EventTypeToString(event.type));
    }
  }
  ImGui::End();
}

auto Event::DrawImGui() const -> void {
  ImGui::Text("%s - Timestamp: %lu", EventTypeToString(type), timestamp);

  DrawVariantImGui();
}

void Event::DrawVariantImGui() const {}

Event::~Event() = default;

GraphicsEvent::GraphicsEvent() {
  auto state = *Graphics::DynamicRendering::TopOfStack;
  auto *snapshot = GetCurrentSnapshot();
  if (snapshot != nullptr) {
    renderState = static_cast<int>(snapshot->renderStates.size());
    snapshot->renderStates.emplace_back(state);
  } else {
    renderState = -1;
  }
}

auto GraphicsEvent::DrawStateImGui() const -> void {
  if (renderState < 0) {
    ImGui::Text("No render state captured.");
    return;
  }

  auto *snapshot = GetCurrentSnapshot();
  if (snapshot == nullptr || renderState >= snapshot->renderStates.size()) {
    ImGui::Text("Invalid render state index.");
    return;
  }

  const auto &state = snapshot->renderStates[renderState];

  ImGui::Text("Render State:");
  ImGui::Indent();
  ImGui::Text("Shader: %s (Module name: %s)", state.shader->name.c_str(),
              state.shader->moduleName.c_str());
  ImGui::Text("Rendertargets:");
  ImGui::Indent();
  for (const auto &target : state.renderTargets) {
    if (target) {
      if (ImGui::TreeNode("%s", "%s",
                          target->texture->GetDebugName().c_str())) {
        ImGui::Text(
            "Format: %s",
            Format::ImageFormatToString(target->texture->format).c_str());
        ImGui::Text("Blend Mode: %s", (target->blendMode.blendEnable != 0U)
                                          ? "Enabled"
                                          : "Disabled");

        auto [isDefault, blendmode, alphamode] =
            Graphics::BlendMode::ToString(target->blendMode);
        if (isDefault) {
          ImGui::Text("Blend Mode: %s, Alpha Mode: %s", blendmode.c_str(),
                      alphamode.c_str());
        } else {
          ImGui::Text(
              "Blend State: \nSrc-Color=%u, \nDst-Color=%u, \nColor Op=%u, "
              "\nSrc Alpha=%u, \nDst Alpha=%u, \nAlpha Op=%u",
              target->blendMode.srcColorBlendFactor,
              target->blendMode.dstColorBlendFactor,
              target->blendMode.colorBlendOp,
              target->blendMode.srcAlphaBlendFactor,
              target->blendMode.dstAlphaBlendFactor,
              target->blendMode.alphaBlendOp);
        }

        ImGui::Text("Clear Value: R=%.2f, G=%.2f, B=%.2f, A=%.2f",
                    target->clearValue.color.float32[0],
                    target->clearValue.color.float32[1],
                    target->clearValue.color.float32[2],
                    target->clearValue.color.float32[3]);
        ImGui::Text("Location: %d", target->location);
        ImGui::Text("Layer: %d", target->layer);
      }
      ImGui::TreePop();
    } else {
      ImGui::Text("- Unable to read rendertarget.");
    }
  }
  ImGui::Unindent();

  std::string cullmodeStr;
  switch (state.cullMode) {
  case VK_CULL_MODE_NONE:
    cullmodeStr = "None";
    break;
  case VK_CULL_MODE_FRONT_BIT:
    cullmodeStr = "Front";
    break;
  case VK_CULL_MODE_BACK_BIT:
    cullmodeStr = "Back";
    break;
  case VK_CULL_MODE_FRONT_AND_BACK:
    cullmodeStr = "All";
    break;
  default:
    cullmodeStr = "Unknown";
    break;
  }
  ImGui::Text("Cull Mode: %s", cullmodeStr.c_str());

  std::string polygonModeStr;
  switch (state.frontFace) {
  case VK_FRONT_FACE_COUNTER_CLOCKWISE:
    polygonModeStr = "Counter Clockwise";
    break;
  case VK_FRONT_FACE_CLOCKWISE:
    polygonModeStr = "Clockwise";
    break;
  case VK_FRONT_FACE_MAX_ENUM:
    break;
  }
  ImGui::Text("Front Face: %s", polygonModeStr.c_str());

  ImGui::Text("Depth Test: %s", state.depthTestEnable ? "Enabled" : "Disabled");
  ImGui::Text("Depth Write: %s",
              state.depthWriteEnable ? "Enabled" : "Disabled");
  ImGui::Text("Stencil Test: %s",
              state.stencilTestEnable ? "Enabled" : "Disabled");

  std::string topologyStr;
  switch (state.primitiveTopology) {
  case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
    topologyStr = "Point List";
    break;
  case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    topologyStr = "Line List";
    break;
  case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    topologyStr = "Line Strip";
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    topologyStr = "Triangle List";
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    topologyStr = "Triangle Strip";
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
    topologyStr = "Triangle Fan";
    break;
  case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    topologyStr = "Line List with Adjacency";
    break;
  case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
    topologyStr = "Line Strip with Adjacency";
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    topologyStr = "Triangle List with Adjacency";
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
    topologyStr = "Triangle Strip with Adjacency";
    break;
  case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
    topologyStr = "Patch List";
    break;
  case VK_PRIMITIVE_TOPOLOGY_MAX_ENUM:
    break;
  }
  ImGui::Text("Primitive Topology: %s", topologyStr.c_str());
  ImGui::Text("Viewport: x=%.2f, y=%.2f, width=%.2f, height=%.2f",
              state.viewport.x, state.viewport.y, state.viewport.width,
              state.viewport.height);
  ImGui::Text("Scissor: offset=(%d, %d), extent=(%u, %u)",
              state.scissor.offset.x, state.scissor.offset.y,
              state.scissor.extent.width, state.scissor.extent.height);
};

/*
struct BufferCreateEvent
struct BufferDestroyEvent
struct BufferUploadEvent
struct StructuredBufferUploadEvent
*/

auto BufferCreateEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Buffer Handle: %lu", bufferHandle);
  ImGui::Text("Memory Handle: %lu", memoryHandle);
  ImGui::Text("Size: %lu", size);
  ImGui::Text("Usage: %u", usage);
  ImGui::Text("Properties: %u", properties);
};

auto BufferDestroyEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Buffer Handle: %lu", bufferHandle);
  ImGui::Text("Memory Handle: %lu", memoryHandle);
};

auto BufferUploadEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Buffer Handle: %lu", bufferHandle);
  ImGui::Text("Memory Handle: %lu", memoryHandle);
  ImGui::Text("Offset: %lu", offset);
  ImGui::Text("Size: %lu", size);
  ImGui::Text("Data Size: %zu bytes", data.size());
};

inline auto DrawComponentData(const std::span<const uint8_t> &data,
                              const BufferComponent &component,
                              VkDeviceSize sourceOffset) -> void {
  auto format = std::get<VkFormat>(component.format);
  auto formatSize = Format::GetSize(format);

  auto span = Utils::Subspan(data, sourceOffset + component.offset,
                             formatSize * component.arraySize);

  if (component.arraySize > 1) {
    if (component.isMatrix) {
      auto rowCount = component.arraySize;
      auto columnCount = Format::GetChannelCount(format);

      const auto &firstRow = Format::ToString(format, span.data());
      ImGui::Text("/ %s \\", firstRow.c_str());

      for (size_t row = 1; row < rowCount - 1; row++) {
        auto rowSpan = Utils::Subspan(span, row * formatSize, formatSize);
        auto rowStr = Format::ToString(format, rowSpan.data());
        ImGui::Text("| %s |", rowStr.c_str());
      }

      if (rowCount > 1) {
        auto lastRowSpan =
            Utils::Subspan(span, (rowCount - 1) * formatSize, formatSize);
        auto lastRowStr = Format::ToString(format, lastRowSpan.data());
        ImGui::Text("\\ %s /", lastRowStr.c_str());
      }
    } else {
      std::string listStr;
      auto channelCount = Format::GetChannelCount(format);

      for (size_t i = 0; i < component.arraySize; i++) {
        auto elementSpan = Utils::Subspan(span, i * formatSize, formatSize);
        auto elementStr = Format::ToString(format, elementSpan.data());
        bool isLast = (i == component.arraySize - 1);

        if (channelCount == 1) {
          listStr += elementStr;
        } else {
          listStr += "(" + elementStr + ")";
        }

        if (!isLast) {
          listStr += ", ";
        }
      }

      ImGui::Text("[#%zu: %s]", component.arraySize, listStr.c_str());
    }
  } else {
    auto elementStr = Format::ToString(format, span.data());
    ImGui::Text("%s", elementStr.c_str());
  }
}

inline auto DrawBufferData(const std::span<const uint8_t> &data,
                           const BufferFormat &format, VkDeviceSize offset,
                           VkDeviceSize size) -> void {
  auto span = Utils::Subspan(data, offset, size);
  if (span.size() % format.GetStride() != 0) {
    ImGui::Text("Warning: Unable to decode structured upload because size is "
                "not a multiple of format stride.");
    return;
  }

  for (const auto &component : format.GetComponents()) {
    ImGui::Text("Component: %s", component.name.c_str());

    auto componentOffset = component.offset;

    if (std::holds_alternative<VkFormat>(component.format)) {
      DrawComponentData(span, component, componentOffset);
    } else {
      auto nestedFormat = std::get<BufferFormat>(component.format);

      for (int element = 0; element < component.arraySize; element++) {
        DrawBufferData(span, nestedFormat, componentOffset,
                       nestedFormat.GetStride());
      }
    }
  }
};

auto StructuredBufferUploadEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Buffer Handle: %lu", bufferHandle);
  ImGui::Text("Format: %s", format.ToString().c_str());

  auto *uploadEvent = this->uploadEvent;
  if (uploadEvent == nullptr) {
    ImGui::Text("No associated BufferUploadEvent found.");
    return;
  }

  auto span =
      Utils::Subspan(uploadEvent->data, uploadEvent->offset, uploadEvent->size);
  if (uploadEvent->size % format.GetStride() != 0) {
    ImGui::Text("Warning: Unable to decode structured upload because size is "
                "not a multiple of format stride.");
    return;
  }

  size_t elementCount = span.size() / format.GetStride();

  if (elementCount == 0) {
    ImGui::Text("No data uploaded.");
    return;
  }

  if (elementCount != 1) {
    ImGui::Text("Element Count: %zu", elementCount);
  }

  ImGui::Separator();

  DrawBufferData(span, format, 0, span.size());
};

/*
struct BufferCopyEvent
struct TextureCreateEvent
struct TextureDestroyEvent
struct TextureUploadEvent
struct TextureCopyEvent
struct PipelineCreateEvent
struct PipelineDestroyEvent
struct DescriptorSetCreateEvent
struct DescriptorSetDestroyEvent
struct SamplerCreateEvent
struct SamplerDestroyEvent
struct ShaderModuleCreateEvent
struct ShaderModuleDestroyEvent
struct DrawEvent
struct DrawIndexedEvent
struct DrawIndirectEvent
struct DrawIndexedIndirectEvent
struct DispatchEvent
struct DispatchIndirectEvent
*/

auto BufferCopyEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Src Buffer Handle: %lu", srcBufferHandle);
  ImGui::Text("Dst Buffer Handle: %lu", dstBufferHandle);
  ImGui::Text("Src Offset: %lu", srcOffset);
  ImGui::Text("Dst Offset: %lu", dstOffset);
  ImGui::Text("Size: %lu", size);
};

auto TextureCreateEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Texture Handle: %lu", textureHandle);
  ImGui::Text("Memory Handle: %lu", memoryHandle);
  ImGui::Text("Width: %u", width);
  ImGui::Text("Height: %u", height);
  ImGui::Text("Depth: %u", depth);
  ImGui::Text("Mip Levels: %u", mipLevels);
  ImGui::Text("Array Layers: %u", arrayLayers);
  ImGui::Text("Format: %s",
              Format::ToString(static_cast<VkFormat>(format)).c_str());
  ImGui::Text("Usage: %u", usage);
  ImGui::Text("Properties: %u", properties);
};

auto TextureDestroyEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Texture Handle: %lu", textureHandle);
  ImGui::Text("Memory Handle: %lu", memoryHandle);
};

auto TextureUploadEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Texture Handle: %lu", textureHandle);
  ImGui::Text("Memory Handle: %lu", memoryHandle);
  ImGui::Text("Width: %u", width);
  ImGui::Text("Height: %u", height);
  ImGui::Text("Depth: %u", depth);
  ImGui::Text("Mip Level: %u", mipLevel);
  ImGui::Text("Array Layer: %u", arrayLayer);
  ImGui::Text("Format: %s",
              Format::ToString(static_cast<VkFormat>(format)).c_str());
  ImGui::Text("Data Size: %zu bytes", dataSize);
};

auto TextureCopyEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Src Texture Handle: %lu", srcTextureHandle);
  ImGui::Text("Dst Texture Handle: %lu", dstTextureHandle);
  ImGui::Text("Src Width: %u", srcWidth);
  ImGui::Text("Src Height: %u", srcHeight);
};

auto DrawEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Vertex Count: %u", vertexCount);
  ImGui::Text("Instance Count: %u", instanceCount);
  ImGui::Text("First Vertex: %u", firstVertex);
  ImGui::Text("First Instance: %u", firstInstance);

  DrawStateImGui();
};

auto DrawIndexedEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Index Count: %u", indexCount);
  ImGui::Text("Instance Count: %u", instanceCount);
  ImGui::Text("First Index: %u", firstIndex);
  ImGui::Text("Vertex Offset: %d", vertexOffset);
  ImGui::Text("First Instance: %u", firstInstance);

  DrawStateImGui();
};

auto DrawIndirectEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Indirect Buffer Handle: %lu", indirectBufferHandle);
  ImGui::Text("Offset: %lu", offset);
  ImGui::Text("Draw Count: %u", drawCount);
  ImGui::Text("Stride: %u", stride);

  DrawStateImGui();
};

auto DrawIndexedIndirectEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Indirect Buffer Handle: %lu", indirectBufferHandle);
  ImGui::Text("Offset: %lu", offset);
  ImGui::Text("Draw Count: %u", drawCount);
  ImGui::Text("Stride: %u", stride);

  DrawStateImGui();
};

auto DispatchEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Group Count X: %u", groupCountX);
  ImGui::Text("Group Count Y: %u", groupCountY);
  ImGui::Text("Group Count Z: %u", groupCountZ);

  DrawStateImGui();
};

auto DispatchIndirectEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Indirect Buffer Handle: %lu", indirectBufferHandle);
  ImGui::Text("Offset: %lu", offset);

  DrawStateImGui();
};

/*
struct SetPipelineEvent
struct SetDescriptorSetEvent
struct SetVertexBufferEvent
struct SetIndexBufferEvent
struct SetPushConstantsEvent
*/

auto SetPipelineEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Pipeline Handle: %lu", pipelineHandle);
  ImGui::Text("Pipeline Bind Point: %u", pipelineBindPoint);
};

auto SetDescriptorSetEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Descriptor Set Handle: %lu", descriptorSetHandle);
  ImGui::Text("Set Index: %u", setIndex);
};

auto SetVertexBufferEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Buffer Handle: %lu", bufferHandle);
  ImGui::Text("Offset: %lu", offset);
};

auto SetIndexBufferEvent::DrawVariantImGui() const -> void {
  ImGui::Text("Buffer Handle: %lu", bufferHandle);
  ImGui::Text("Offset: %lu", offset);
  ImGui::Text("Index Type: %u", indexType);
};

//
} // namespace Graphics::Snapshot