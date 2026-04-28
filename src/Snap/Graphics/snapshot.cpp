#include "snapshot.hpp"
#include "Graphics/blendmode.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/format.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/utils.hpp"
#include <cstdint>
#include <format>
#include <imgui.h>
#include <memory>
#include <string>
#include <variant>
#include <vulkan/vulkan_core.h>

namespace Graphics::Snapshot {

auto EventTypeToString(EventType type) -> const char * {
  switch (type) {
  case EventType::Create_Buffer:
    return "Create buffer";
  case EventType::Create_Texture:
    return "Create texture";
  case EventType::Create_Pipeline:
    return "Create pipeline";
  case EventType::Create_DescriptorSet:
    return "Create descriptor set";
  case EventType::Create_Sampler:
    return "Create sampler";
  case EventType::Create_ShaderModule:
    return "Create shader module";
  case EventType::Destroy_Buffer:
    return "Destroy buffer";
  case EventType::Destroy_Texture:
    return "Destroy texture";
  case EventType::Destroy_Pipeline:
    return "Destroy pipeline";
  case EventType::Destroy_DescriptorSet:
    return "Destroy descriptor set";
  case EventType::Destroy_Sampler:
    return "Destroy sampler";
  case EventType::Destroy_ShaderModule:
    return "Destroy shader module";
  case EventType::Structured_Buffer_Upload:
    return "Structured buffer upload";
  case EventType::Upload_To_Buffer:
    return "Upload to buffer";
  case EventType::Upload_To_Texture:
    return "Upload to texture";
  case EventType::Copy_Buffer_To_Buffer:
    return "Copy buffer to buffer";
  case EventType::Copy_Buffer_To_Texture:
    return "Copy buffer to texture";
  case EventType::Copy_Texture_To_Buffer:
    return "Copy texture to buffer";
  case EventType::Copy_Texture_To_Texture:
    return "Copy texture to texture";
  case EventType::Draw:
    return "Draw";
  case EventType::DrawIndexed:
    return "Draw indexed";
  case EventType::DrawIndirect:
    return "Draw indirect";
  case EventType::DrawIndexedIndirect:
    return "Draw indexed indirect";
  case EventType::Dispatch:
    return "Dispatch";
  case EventType::DispatchIndirect:
    return "Dispatch indirect";
  case EventType::Set_Pipeline:
    return "Set pipeline";
  case EventType::Set_DescriptorSet:
    return "Set descriptor set";
  case EventType::Set_VertexBuffer:
    return "Set vertex buffer";
  case EventType::Set_IndexBuffer:
    return "Set index buffer";
  case EventType::Set_PushConstants:
    return "Set push constants";
  case EventType::Barrier:
    return "Barrier";
  default:
    return "Unknown";
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto EventTypeFromString(const std::string &str) -> EventType {
  // clang-format off
  if (str == "Create buffer") { return EventType::Create_Buffer; }
  if (str == "Create texture") { return EventType::Create_Texture; }
  if (str == "Create pipeline") { return EventType::Create_Pipeline; }
  if (str == "Create descriptor set") { return EventType::Create_DescriptorSet; }
  if (str == "Create sampler") { return EventType::Create_Sampler; }
  if (str == "Create shader module") { return EventType::Create_ShaderModule; }
  if (str == "Destroy buffer") { return EventType::Destroy_Buffer; }
  if (str == "Destroy texture") { return EventType::Destroy_Texture; }
  if (str == "Destroy pipeline") { return EventType::Destroy_Pipeline; }
  if (str == "Destroy descriptor set") { return EventType::Destroy_DescriptorSet; }
  if (str == "Destroy sampler") { return EventType::Destroy_Sampler; }
  if (str == "Destroy shader module") { return EventType::Destroy_ShaderModule; }
  if (str == "Structured buffer upload") { return EventType::Structured_Buffer_Upload; }
  if (str == "Upload to buffer") { return EventType::Upload_To_Buffer; }
  if (str == "Upload to texture") { return EventType::Upload_To_Texture; }
  if (str == "Copy buffer to buffer") { return EventType::Copy_Buffer_To_Buffer; }
  if (str == "Copy buffer to texture") { return EventType::Copy_Buffer_To_Texture; }
  if (str == "Copy texture to buffer") { return EventType::Copy_Texture_To_Buffer; }
  if (str == "Copy texture to texture") { return EventType::Copy_Texture_To_Texture; }
  if (str == "Draw") { return EventType::Draw; }
  if (str == "Draw indexed") { return EventType::DrawIndexed; }
  if (str == "Draw indirect") { return EventType::DrawIndirect; }
  if (str == "Draw indexed indirect") { return EventType::DrawIndexedIndirect; }
  if (str == "Dispatch") { return EventType::Dispatch; }
  if (str == "Dispatch indirect") { return EventType::DispatchIndirect; }
  if (str == "Set pipeline") { return EventType::Set_Pipeline; }
  if (str == "Set descriptor set") { return EventType::Set_DescriptorSet; }
  if (str == "Set vertex buffer") { return EventType::Set_VertexBuffer; }
  if (str == "Set index buffer") { return EventType::Set_IndexBuffer; }
  if (str == "Set push constants") { return EventType::Set_PushConstants; }
  if (str == "Barrier") { return EventType::Barrier; }
  return EventType::Unknown;
  // clang-format on
}

auto GetInternalSnapshot() -> ThreadSnapshot & {
  thread_local ThreadSnapshot currentSnapshot;
  return currentSnapshot;
}

auto GetCurrentSnapshot() -> ThreadSnapshot * {
  auto &currentSnapshot = GetInternalSnapshot();

  if (!currentSnapshot.active) {
    return nullptr;
  }

  return &currentSnapshot;
}

auto StartSnapshot() -> void {
  auto &currentSnapshot = GetInternalSnapshot();

  currentSnapshot.events.clear();
  currentSnapshot.renderStates.clear();
  currentSnapshot.threadId =
      std::hash<std::thread::id>{}(std::this_thread::get_id());
  currentSnapshot.threadName = Graphics::ContextDebugname;
  currentSnapshot.active = true;
}
auto EndSnapshot() -> void {
  auto &currentSnapshot = GetInternalSnapshot();
  currentSnapshot.active = false;
}
auto RenderSnapshot(const ThreadSnapshot &snapshot) -> void {
  int index = 1;
  if (ImGui::Begin("Snapshot Viewer")) {
    if (ImGui::BeginTable("Events", 1,
                          ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_BordersInnerV)) {
      for (const auto &event : snapshot.events) {
        ImGui::PushID(index++);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        if (ImGui::TreeNode("", "%s", EventTypeToString(event->type))) {

          event->DrawImGui(&snapshot);
          ImGui::TreePop();
        }
        ImGui::PopID();
      }

      ImGui::EndTable();
    }
  }
  ImGui::End();
}

auto Event::DrawImGui(ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Timestamp: %lu", timestamp);

  DrawVariantImGui(parent);
}

void Event::DrawVariantImGui(ThreadSnapshot const *parent) const {}

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

inline void BooleanFlag(const char *label, bool value,
                        const char *trueLabel = "Enabled", // NOLINT
                        const char *falseLabel = "Disabled") {
  ImGui::Text("%s", label);
  ImGui::SameLine();
  if (value) {
    ImGui::Text("%s", trueLabel);
  } else {
    ImGui::TextDisabled("%s", falseLabel);
  }
}

inline auto DrawRendertargetImGui(const DynamicRendering::RenderTarget *target,
                                  int index) -> void {
  if (target != nullptr) {
    if (ImGui::TreeNode("%s", "%s", target->texture->GetDebugName().c_str())) {
      ImGui::Text("Format: %s",
                  Format::ImageFormatToString(target->texture->format).c_str());

      auto blend = target->blendMode;
      BooleanFlag("Blend Mode:", blend.blendEnable != 0U);

      auto [isDefault, blendmode, alphamode] = BlendMode::ToString(blend);
      if (isDefault) {
        ImGui::Text("Blend Mode: %s, Alpha Mode: %s", blendmode.c_str(),
                    alphamode.c_str());
        ImGui::SetItemTooltip(
            "Blend State:\nSrc-Color=%s\nDst-Color=%s\nColor Op=%s"
            "\nSrc Alpha=%s\nDst Alpha=%s\nAlpha Op=%s",
            BlendMode::ToString(blend.srcColorBlendFactor).c_str(),
            BlendMode::ToString(blend.dstColorBlendFactor).c_str(),
            BlendMode::ToString(blend.colorBlendOp).c_str(),
            BlendMode::ToString(blend.srcAlphaBlendFactor).c_str(),
            BlendMode::ToString(blend.dstAlphaBlendFactor).c_str(),
            BlendMode::ToString(blend.alphaBlendOp).c_str());
      } else {
        ImGui::Text("Blend State:\nSrc-Color=%s\nDst-Color=%s\nColor Op=%s"
                    "\nSrc Alpha=%s\nDst Alpha=%s\nAlpha Op=%s",
                    BlendMode::ToString(blend.srcColorBlendFactor).c_str(),
                    BlendMode::ToString(blend.dstColorBlendFactor).c_str(),
                    BlendMode::ToString(blend.colorBlendOp).c_str(),
                    BlendMode::ToString(blend.srcAlphaBlendFactor).c_str(),
                    BlendMode::ToString(blend.dstAlphaBlendFactor).c_str(),
                    BlendMode::ToString(blend.alphaBlendOp).c_str());
      }

      ImGui::Text("Clear Value: R=%.2f, G=%.2f, B=%.2f, A=%.2f",
                  target->clearValue.color.float32[0],
                  target->clearValue.color.float32[1],
                  target->clearValue.color.float32[2],
                  target->clearValue.color.float32[3]);
      auto location = target->location;
      if (location == -1) {
        location = index;
      }

      ImGui::Text("Location: %d", location);
      ImGui::Text("Layer: %d", target->layer);

      ImGui::TreePop();
    }
  } else {
    ImGui::Text("- Unable to read rendertarget.");
  }
}

auto GraphicsEvent::DrawStateImGui(ThreadSnapshot const *parent) const -> void {
  if (renderState < 0) {
    ImGui::Text("No render state captured.");
    return;
  }

  if (parent == nullptr || renderState >= parent->renderStates.size()) {
    ImGui::Text("Invalid render state index.");
    return;
  }

  const auto &state = parent->renderStates[renderState];

  ImGui::Text("Render State:");
  ImGui::Indent();
  ImGui::Text("Shader: %s (Module name: %s)", state.shader->name.c_str(),
              state.shader->moduleName.c_str());
  ImGui::Text("Rendertargets:");
  ImGui::Indent();
  int rtIndex = 0;
  for (const auto &target : state.renderTargets) {
    ImGui::PushID(rtIndex++);
    DrawRendertargetImGui(target.get(), rtIndex - 1);
    ImGui::PopID();
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

  BooleanFlag("Depth Test:", state.depthTestEnable);
  BooleanFlag("Depth Write:", state.depthWriteEnable);
  BooleanFlag("Stencil Test:", state.stencilTestEnable);

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
  if (state.hasViewport) {
    ImGui::Text("Viewport: x=%.2f, y=%.2f, width=%.2f, height=%.2f",
                state.viewport.x, state.viewport.y, state.viewport.width,
                state.viewport.height);
  } else {
    auto texture = state.renderTargets.at(0)->texture;
    ImGui::Text("Viewport: x=0.00, y=0.00, width=%.2u, height=%.2u (Default)",
                texture->GetWidth(), texture->GetHeight());
  }

  if (state.hasScissor) {
    ImGui::Text("Scissor: x=%u, y=%u, width=%u, height=%u",
                state.scissor.offset.x, state.scissor.offset.y,
                state.scissor.extent.width, state.scissor.extent.height);
  } else {
    auto texture = state.renderTargets.at(0)->texture;
    ImGui::Text("Scissor: x=0, y=0, width=%u, height=%u (Default)",
                texture->GetWidth(), texture->GetHeight());
  }
};

/*
struct BufferCreateEvent
struct BufferDestroyEvent
struct BufferUploadEvent
struct StructuredBufferUploadEvent
*/

auto BufferCreateEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Buffer Handle: %p", bufferHandle);
  ImGui::Text("Memory Handle: %p", memoryHandle);
  ImGui::Text("Size: %lu", size);
  ImGui::Text("Usage: %u", usage);
  ImGui::Text("Properties: %u", properties);
};

auto BufferDestroyEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Buffer Handle: %p", bufferHandle);
  ImGui::Text("Memory Handle: %p", memoryHandle);
};

auto BufferUploadEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Buffer Handle: %p", bufferHandle);
  ImGui::Text("Memory Handle: %p", memoryHandle);
  ImGui::Text("Offset: %lu", offset);
  ImGui::Text("Size: %lu", size);
  ImGui::Text("Data Size: %zu bytes", data.size());
};

inline auto
DrawComponentData(const std::span<const uint8_t> &data,
                  const BufferComponent &component, // NOLINTNEXTLINE
                  VkDeviceSize sourceOffset, VkDeviceSize rawOffset) -> void {
  auto format = std::get<VkFormat>(component.format);
  auto formatSize = Format::GetSize(format);

  auto finalOffset = component.offset + sourceOffset;

  bool withinSpan =
      finalOffset + (formatSize * component.arraySize) <= data.size();
  if (finalOffset < rawOffset || !withinSpan) {
    ImGui::Text("No data available.");
    return;
  }

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

        if (channelCount == 1) {
          listStr += "  " + elementStr;
        } else {
          listStr += "  (" + elementStr + ")";
        }

        listStr += ";\n";
      }

      ImGui::Text("[#%zu:\n%s]", component.arraySize, listStr.c_str());
    }
  } else {
    auto elementStr = Format::ToString(format, span.data());
    ImGui::Text("%s", elementStr.c_str());
  }
}

inline auto DrawBufferData(const std::span<const uint8_t> &data,
                           const BufferFormat &format,
                           VkDeviceSize offset, // NOLINTNEXTLINE
                           VkDeviceSize size, VkDeviceSize rawOffset) -> void {

  if (offset + size > data.size()) {
    ImGui::Text("No more data available.");
    return;
  }

  auto span = Utils::Subspan(data, offset, size);

  for (const auto &component : format.GetComponents()) {
    ImGui::Text("Component: %s", component.name.c_str());

    auto componentOffset = component.offset;

    if (std::holds_alternative<VkFormat>(component.format)) {
      DrawComponentData(span, component, componentOffset, rawOffset);
    } else {
      auto nestedFormat = std::get<BufferFormat>(component.format);

      for (int element = 0; element < component.arraySize; element++) {
        DrawBufferData(span, nestedFormat, componentOffset,
                       nestedFormat.GetStride(), rawOffset);
      }
    }
  }
};

auto StructuredBufferUploadEvent::DrawVariantImGui(
    ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Buffer Handle: %p", bufferHandle);
  ImGui::Text("Format: %s", format.ToString().c_str());

  auto *uploadEvent = this->uploadEvent;
  if (uploadEvent == nullptr) {
    ImGui::Text("No associated BufferUploadEvent found.");
    return;
  }

  if (uploadEvent->size % format.GetStride() != 0) {
    ImGui::Text("Warning: Structured data upload of size (%lu) is "
                "not a multiple of format stride (%lu).",
                uploadEvent->size, format.GetStride());
  }

  ImGui::Separator();

  DrawBufferData(uploadEvent->data, format, 0, uploadEvent->size,
                 uploadEvent->offset);
};

/*
struct BufferCopyEvent
struct TextureCreateEvent
struct TextureDestroyEvent
struct TextureUploadEvent
struct TextureCopyEvent

*/

auto BufferCopyEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Src Buffer Handle: %p", srcBufferHandle);
  ImGui::Text("Dst Buffer Handle: %p", dstBufferHandle);
  ImGui::Text("Src Offset: %lu", srcOffset);
  ImGui::Text("Dst Offset: %lu", dstOffset);
  ImGui::Text("Size: %lu", size);
};

auto TextureCreateEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Texture Handle: %p", textureHandle);
  ImGui::Text("Memory Handle: %p", memoryHandle);
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

auto TextureDestroyEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Texture Handle: %p", textureHandle);
  ImGui::Text("Memory Handle: %p", memoryHandle);
};

auto TextureUploadEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Texture Handle: %p", textureHandle);
  ImGui::Text("Memory Handle: %p", memoryHandle);
  ImGui::Text("Width: %u", width);
  ImGui::Text("Height: %u", height);
  ImGui::Text("Depth: %u", depth);
  ImGui::Text("Mip Level: %u", mipLevel);
  ImGui::Text("Array Layer: %u", arrayLayer);
  ImGui::Text("Format: %s",
              Format::ToString(static_cast<VkFormat>(format)).c_str());
  ImGui::Text("Data Size: %zu bytes", dataSize);
};

auto TextureCopyEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Src Texture Handle: %p", srcTextureHandle);
  ImGui::Text("Dst Texture Handle: %p", dstTextureHandle);
  ImGui::Text("Src Width: %u", srcWidth);
  ImGui::Text("Src Height: %u", srcHeight);
};

/*
struct PipelineCreateEvent
struct PipelineDestroyEvent
*/

auto PipelineCreateEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Pipeline Handle: %p", pipelineHandle);
  ImGui::Text("Pipeline Type: %u", pipelineType);
};

auto PipelineDestroyEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Pipeline Handle: %p", pipelineHandle);
};

/*
struct DescriptorSetCreateEvent
struct DescriptorSetDestroyEvent

*/

auto DescriptorSetCreateEvent::DrawVariantImGui(
    ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Descriptor Set Handle: %p", descriptorSetHandle);
};

auto DescriptorSetDestroyEvent::DrawVariantImGui(
    ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Descriptor Set Handle: %p", descriptorSetHandle);
};

/*
struct SamplerCreateEvent
struct SamplerDestroyEvent

*/

auto SamplerCreateEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Sampler Handle: %p", samplerHandle);
};

auto SamplerDestroyEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Sampler Handle: %p", samplerHandle);
};

/*
struct ShaderModuleCreateEvent
struct ShaderModuleDestroyEvent
struct DrawEvent
struct DrawIndexedEvent
struct DrawIndirectEvent
struct DrawIndexedIndirectEvent
struct DispatchEvent
struct DispatchIndirectEvent
*/

auto ShaderModuleCreateEvent::DrawVariantImGui(
    ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Shader Module Handle: %p", shaderModuleHandle);
  ImGui::Text("Module Name: %s", moduleName.c_str());
};

auto ShaderModuleDestroyEvent::DrawVariantImGui(
    ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Shader Module Handle: %p", shaderModuleHandle);
};

auto DrawEvent::DrawVariantImGui(ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Vertex Count: %u", vertexCount);
  ImGui::Text("Instance Count: %u", instanceCount);
  ImGui::Text("First Vertex: %u", firstVertex);
  ImGui::Text("First Instance: %u", firstInstance);

  DrawStateImGui(parent);
};

auto DrawIndexedEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Index Count: %u", indexCount);
  ImGui::Text("Instance Count: %u", instanceCount);
  ImGui::Text("First Index: %u", firstIndex);
  ImGui::Text("Vertex Offset: %d", vertexOffset);
  ImGui::Text("First Instance: %u", firstInstance);

  DrawStateImGui(parent);
};

auto DrawIndirectEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Indirect Buffer Handle: %p", indirectBufferHandle);
  ImGui::Text("Offset: %lu", offset);
  ImGui::Text("Draw Count: %u", drawCount);
  ImGui::Text("Stride: %u", stride);

  DrawStateImGui(parent);
};

auto DrawIndexedIndirectEvent::DrawVariantImGui(
    ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Indirect Buffer Handle: %p", indirectBufferHandle);
  ImGui::Text("Offset: %lu", offset);
  ImGui::Text("Draw Count: %u", drawCount);
  ImGui::Text("Stride: %u", stride);

  DrawStateImGui(parent);
};

auto DispatchEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Group Count X: %u", groupCountX);
  ImGui::Text("Group Count Y: %u", groupCountY);
  ImGui::Text("Group Count Z: %u", groupCountZ);

  DrawStateImGui(parent);
};

auto DispatchIndirectEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Indirect Buffer Handle: %p", indirectBufferHandle);
  ImGui::Text("Offset: %lu", offset);

  DrawStateImGui(parent);
};

/*
struct SetPipelineEvent
struct SetDescriptorSetEvent
struct SetVertexBufferEvent
struct SetIndexBufferEvent
struct SetPushConstantsEvent
*/

auto SetPipelineEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Pipeline Handle: %p", pipelineHandle);
  ImGui::Text("Pipeline Bind Point: %u", pipelineBindPoint);
};

auto SetDescriptorSetEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Descriptor Set Handle: %p", descriptorSetHandle);
  ImGui::Text("Set Index: %u", setIndex);
};

auto SetVertexBufferEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Buffer Handle: %p", bufferHandle);
  ImGui::Text("Offset: %lu", offset);
};

auto SetIndexBufferEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Buffer Handle: %p", bufferHandle);
  ImGui::Text("Offset: %lu", offset);
  ImGui::Text("Index Type: %u", indexType);
};

inline auto PipelineStageFlag2ToString(VkPipelineStageFlags2 flag)
    -> std::string {
  switch (flag) {
    // clang-format off
  case VK_PIPELINE_STAGE_2_NONE: {return "NONE";}
  case VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT: {return "TOP_OF_PIPE_BIT";}
  case VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT: {return "DRAW_INDIRECT_BIT";}
  case VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT: {return "VERTEX_INPUT_BIT";}
  case VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT: {return "VERTEX_SHADER_BIT";}
  case VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT: {return "TESSELLATION_CONTROL_SHADER_BIT";}
  case VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT: {return "TESSELLATION_EVALUATION_SHADER_BIT";}
  case VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT: {return "GEOMETRY_SHADER_BIT";}
  case VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT: {return "FRAGMENT_SHADER_BIT";}
  case VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT: {return "EARLY_FRAGMENT_TESTS_BIT";}
  case VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT: {return "LATE_FRAGMENT_TESTS_BIT";}
  case VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT: {return "COLOR_ATTACHMENT_OUTPUT_BIT";}
  case VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT: {return "COMPUTE_SHADER_BIT";}
  case VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT: {return "ALL_TRANSFER_BIT";}
  case VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT: {return "BOTTOM_OF_PIPE_BIT";}
  case VK_PIPELINE_STAGE_2_HOST_BIT: {return "HOST_BIT";}
  case VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT: {return "ALL_GRAPHICS_BIT";}
  case VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT: {return "ALL_COMMANDS_BIT";}
  case VK_PIPELINE_STAGE_2_COPY_BIT: {return "COPY_BIT";}
  case VK_PIPELINE_STAGE_2_RESOLVE_BIT: {return "RESOLVE_BIT";}
  case VK_PIPELINE_STAGE_2_BLIT_BIT: {return "BLIT_BIT";}
  case VK_PIPELINE_STAGE_2_CLEAR_BIT: {return "CLEAR_BIT";}
  case VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT: {return "INDEX_INPUT_BIT";}
  case VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT: {return "VERTEX_ATTRIBUTE_INPUT_BIT";}
  case VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT: {return "PRE_RASTERIZATION_SHADERS_BIT";}
  case VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR: {return "VIDEO_DECODE_BIT_KHR";}
  case VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR: {return "VIDEO_ENCODE_BIT_KHR";}
  case VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT: {return "TRANSFORM_FEEDBACK_BIT_EXT";}
  case VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT: {return "CONDITIONAL_RENDERING_BIT_EXT";}
  case VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_NV: {return "COMMAND_PREPROCESS_BIT_NV";}
  case VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR: {return "FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR";}
  case VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR: {return "ACCELERATION_STRUCTURE_BUILD_BIT_KHR";}
  case VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR: {return "RAY_TRACING_SHADER_BIT_KHR";}
  case VK_PIPELINE_STAGE_2_FRAGMENT_DENSITY_PROCESS_BIT_EXT: {return "FRAGMENT_DENSITY_PROCESS_BIT_EXT";}
  case VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT: {return "TASK_SHADER_BIT_EXT";}
  case VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT: {return "MESH_SHADER_BIT_EXT";}
  case VK_PIPELINE_STAGE_2_SUBPASS_SHADER_BIT_HUAWEI: {return "SUBPASS_SHADER_BIT_HUAWEI";}
  // clang-format on
  default:
    return std::format("Unknown Pipeline Stage Flag: {}", flag);
  }
}

inline auto AccessFlag2ToString(VkAccessFlags2 flag) {
  switch (flag) {
    // clang-format off

  case VK_ACCESS_2_NONE: { return "NONE"; }
  case VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT: { return "INDIRECT_COMMAND_READ_BIT"; }
  case VK_ACCESS_2_INDEX_READ_BIT: { return "INDEX_READ_BIT"; }
  case VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT: { return "VERTEX_ATTRIBUTE_READ_BIT"; }
  case VK_ACCESS_2_UNIFORM_READ_BIT: { return "UNIFORM_READ_BIT"; }
  case VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT: { return "INPUT_ATTACHMENT_READ_BIT"; }
  case VK_ACCESS_2_SHADER_READ_BIT: { return "SHADER_READ_BIT"; }
  case VK_ACCESS_2_SHADER_WRITE_BIT: { return "SHADER_WRITE_BIT"; }
  case VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT: { return "COLOR_ATTACHMENT_READ_BIT"; }
  case VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT: { return "COLOR_ATTACHMENT_WRITE_BIT"; }
  case VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT: { return "DEPTH_STENCIL_ATTACHMENT_READ_BIT"; }
  case VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT: { return "DEPTH_STENCIL_ATTACHMENT_WRITE_BIT"; }
  case VK_ACCESS_2_TRANSFER_READ_BIT: { return "TRANSFER_READ_BIT"; }
  case VK_ACCESS_2_TRANSFER_WRITE_BIT: { return "TRANSFER_WRITE_BIT"; }
  case VK_ACCESS_2_HOST_READ_BIT: { return "HOST_READ_BIT"; }
  case VK_ACCESS_2_HOST_WRITE_BIT: { return "HOST_WRITE_BIT"; }
  case VK_ACCESS_2_MEMORY_READ_BIT: { return "MEMORY_READ_BIT"; }
  case VK_ACCESS_2_MEMORY_WRITE_BIT: { return "MEMORY_WRITE_BIT"; }
  case VK_ACCESS_2_SHADER_SAMPLED_READ_BIT: { return "SHADER_SAMPLED_READ_BIT"; }
  case VK_ACCESS_2_SHADER_STORAGE_READ_BIT: { return "SHADER_STORAGE_READ_BIT"; }
  case VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT: { return "SHADER_STORAGE_WRITE_BIT"; }
  case VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR: { return "VIDEO_DECODE_READ_BIT_KHR"; }
  case VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR: { return "VIDEO_DECODE_WRITE_BIT_KHR"; }
  case VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR: { return "VIDEO_ENCODE_READ_BIT_KHR"; }
  case VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR: { return "VIDEO_ENCODE_WRITE_BIT_KHR"; }
  case VK_ACCESS_2_SHADER_TILE_ATTACHMENT_READ_BIT_QCOM: { return "SHADER_TILE_ATTACHMENT_READ_BIT_QCOM"; }
  case VK_ACCESS_2_SHADER_TILE_ATTACHMENT_WRITE_BIT_QCOM: { return "SHADER_TILE_ATTACHMENT_WRITE_BIT_QCOM"; }
  case VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT: { return "TRANSFORM_FEEDBACK_WRITE_BIT_EXT"; }
  case VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT: { return "TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT"; }
  case VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT: { return "TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT"; }
  case VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT: { return "CONDITIONAL_RENDERING_READ_BIT_EXT"; }
  case VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_EXT: { return "COMMAND_PREPROCESS_READ_BIT_EXT"; }
  case VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_EXT: { return "COMMAND_PREPROCESS_WRITE_BIT_EXT"; }
  case VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR: { return "FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR"; }
  case VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR: { return "ACCELERATION_STRUCTURE_READ_BIT_KHR"; }
  case VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR: { return "ACCELERATION_STRUCTURE_WRITE_BIT_KHR"; }
  case VK_ACCESS_2_FRAGMENT_DENSITY_MAP_READ_BIT_EXT: { return "FRAGMENT_DENSITY_MAP_READ_BIT_EXT"; }
  case VK_ACCESS_2_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT: { return "COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT"; }
  case VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT: { return "DESCRIPTOR_BUFFER_READ_BIT_EXT"; }
  case VK_ACCESS_2_INVOCATION_MASK_READ_BIT_HUAWEI: { return "INVOCATION_MASK_READ_BIT_HUAWEI"; }
  case VK_ACCESS_2_SHADER_BINDING_TABLE_READ_BIT_KHR: { return "SHADER_BINDING_TABLE_READ_BIT_KHR"; }
  case VK_ACCESS_2_MICROMAP_READ_BIT_EXT: { return "MICROMAP_READ_BIT_EXT"; }
  case VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT: { return "MICROMAP_WRITE_BIT_EXT"; }
  case VK_ACCESS_2_OPTICAL_FLOW_READ_BIT_NV: { return "OPTICAL_FLOW_READ_BIT_NV"; }
  case VK_ACCESS_2_OPTICAL_FLOW_WRITE_BIT_NV: { return "OPTICAL_FLOW_WRITE_BIT_NV"; }
  case VK_ACCESS_2_DATA_GRAPH_READ_BIT_ARM: { return "DATA_GRAPH_READ_BIT_ARM"; }
  case VK_ACCESS_2_DATA_GRAPH_WRITE_BIT_ARM: { return "DATA_GRAPH_WRITE_BIT_ARM"; }
  // clang-format on
  default:
    return "Unknown Access Flag";
  }
}

auto BarrierEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {

  ImGui::Text("Source Stages:");
  ImGui::Indent();
  if (sync.srcStages == 0) {
    ImGui::Text("None");
  } else {
    for (const auto &stage : Utils::BitMaskRange(sync.srcStages)) {
      ImGui::Text("%s", PipelineStageFlag2ToString(stage).c_str());
    }
  }
  ImGui::Unindent();
  ImGui::Text("Source Access:");
  ImGui::Indent();
  if (sync.srcAccess == 0) {
    ImGui::Text("None");
  } else {
    for (const auto &access : Utils::BitMaskRange(sync.srcAccess)) {
      ImGui::Text("%s", AccessFlag2ToString(access));
    }
  }
  ImGui::Unindent();
  ImGui::Text("Destination Stages:");
  ImGui::Indent();
  if (sync.dstStages == 0) {
    ImGui::Text("None");
  } else {
    for (const auto &stage : Utils::BitMaskRange(sync.dstStages)) {
      ImGui::Text("%s", PipelineStageFlag2ToString(stage).c_str());
    }
  }
  ImGui::Unindent();
  ImGui::Text("Destination Access:");
  ImGui::Indent();
  if (sync.dstAccess == 0) {
    ImGui::Text("None");
  } else {
    for (const auto &access : Utils::BitMaskRange(sync.dstAccess)) {
      ImGui::Text("%s", AccessFlag2ToString(access));
    }
  }
  ImGui::Unindent();
};

//
} // namespace Graphics::Snapshot