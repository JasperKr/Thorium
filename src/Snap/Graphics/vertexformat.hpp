#pragma once

#include "Graphics/format.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/Helpers/hasher.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "Modules/console.hpp"
#include "vulkan/vulkan_core.h"

namespace Graphics {

struct VertexComponent {
  std::string name;
  uint32_t location{};
  uint32_t binding{};
  VkFormat format{};
  uint32_t offset{};
};

struct VertexFormat {
  explicit VertexFormat(std::vector<VertexComponent> attributes)
      : Attributes(std::move(attributes)) {
    ConstructBindings();
  }

  VertexFormat() = default;
  VertexFormat(const VertexFormat &other) = default;
  VertexFormat(VertexFormat &&other) noexcept = default;
  auto operator=(const VertexFormat &other) -> VertexFormat & = default;
  auto operator=(VertexFormat &&other) noexcept -> VertexFormat & = default;
  ~VertexFormat() = default;

public:
  [[nodiscard]] auto GetAttributes() const
      -> const std::vector<VertexComponent> & {
    return Attributes;
  }

  [[nodiscard]] auto GetVkAttributes()
      -> std::vector<VkVertexInputAttributeDescription> & {
    if (!constructedBindings) {
      ConstructBindings();
    }
    return VkAttributes;
  }

  [[nodiscard]] auto GetVkAttributes2()
      -> std::vector<VkVertexInputAttributeDescription2EXT> & {
    if (!constructedBindings) {
      ConstructBindings();
    }
    return VkAttributes2;
  }

  [[nodiscard]] auto GetBindings()
      -> const std::vector<VkVertexInputBindingDescription> & {
    if (!constructedBindings) {
      ConstructBindings();
    }
    return Bindings;
  }

  [[nodiscard]] auto GetStride(uint32_t binding) -> uint32_t {
    if (!constructedBindings) {
      ConstructBindings();
    }
    assert(binding < Bindings.size());
    return Bindings[binding].stride;
  }

  [[nodiscard]] auto ToString() const -> std::string {
    std::string result = "VertexFormat:\n";
    for (const auto &attribute : Attributes) {
      auto formatStr = Format::ToString(attribute.format);
      auto offsetStr = std::to_string(attribute.offset);
      result = std::format("{}  - {}: {} - offset: {}\n", result,
                           attribute.name, formatStr, offsetStr);
    }
    return result;
  }

  auto BindDynamicInputState(VkCommandBuffer commandBuffer) -> void {
    thread_local VkVertexInputBindingDescription2EXT vertexInputInfo = {};
    const auto &bindings = GetBindings();
    auto &attributes = GetVkAttributes2();

    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
    vertexInputInfo.binding = 0;
    vertexInputInfo.stride = bindings.at(0).stride;
    vertexInputInfo.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexInputInfo.divisor = 1;

    vkCmdSetVertexInputEXT(commandBuffer, 1, &vertexInputInfo,
                           static_cast<uint32_t>(attributes.size()),
                           attributes.data());
  }

  auto operator==(const VertexFormat &other) const -> bool {
    if (Attributes.size() != other.Attributes.size()) {
      return false;
    }

    for (size_t i = 0; i < Attributes.size(); ++i) {
      if (Attributes[i].location != other.Attributes[i].location ||
          Attributes[i].binding != other.Attributes[i].binding ||
          Attributes[i].format != other.Attributes[i].format ||
          Attributes[i].offset != other.Attributes[i].offset ||
          Attributes[i].name != other.Attributes[i].name) {
        return false;
      }
    }

    return true;
  }

  auto operator!=(const VertexFormat &other) const -> bool {
    return !(*this == other);
  }

  [[nodiscard]] auto GetHash() const -> size_t {
    Hash::Hasher hasher{};
    for (const auto &attribute : Attributes) {
      hasher.Add(std::hash<std::string>()(attribute.name));
      hasher.Add(std::hash<uint32_t>()(attribute.location));
      hasher.Add(std::hash<uint32_t>()(attribute.binding));
      hasher.Add(std::hash<uint32_t>()(attribute.format));
      hasher.Add(std::hash<uint32_t>()(attribute.offset));
    }
    return hasher.Get();
  }

private:
  bool constructedBindings = false;
  std::vector<VertexComponent> Attributes;

  void ConstructBindings() {
    std::ranges::sort(Attributes,
                      [](const VertexComponent &first,
                         const VertexComponent &second) -> bool {
                        if (first.binding != second.binding) {
                          return first.binding < second.binding;
                        }
                        return first.location < second.location;
                      });

    for (auto &component : Attributes) {
      // Sanity check for invalid binding numbers
      // real max is higher but i expect to use like 2 anyways
      assert(component.binding <= 255 &&
             "Vertex attribute binding exceeds maximum of 255");
      Bindings.resize((std::max)(Bindings.size(),
                                 static_cast<size_t>(component.binding + 1)));

      Bindings[component.binding] = VkVertexInputBindingDescription{
          .binding = component.binding,
          .stride = 0,
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      };
    }

    for (auto &component : Attributes) {
      auto formatSize = Format::GetSize(component.format);

      if (formatSize == 0) {
        PrintFatal(
            "Vertex attribute '{}' has unsupported or unknown format: {}",
            component.name, Format::ToString(component.format));
      }
      assert(formatSize > 0 &&
             "Vertex attribute has unsupported or unknown format: ");

      component.offset = Bindings[component.binding].stride;
      Bindings[component.binding].stride += formatSize;
    }

    VkAttributes.reserve(Attributes.size());
    for (const auto &component : Attributes) {
      VkAttributes.emplace_back(
          VkVertexInputAttributeDescription{.location = component.location,
                                            .binding = component.binding,
                                            .format = component.format,
                                            .offset = component.offset});
    }

    VkAttributes2.reserve(Attributes.size());
    for (const auto &component : Attributes) {
      VkAttributes2.emplace_back(VkVertexInputAttributeDescription2EXT{
          .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
          .location = component.location,
          .binding = component.binding,
          .format = component.format,
          .offset = component.offset,
      });
    }

    constructedBindings = true;
  }

  std::vector<VkVertexInputBindingDescription> Bindings;
  std::vector<VkVertexInputAttributeDescription> VkAttributes;
  std::vector<VkVertexInputAttributeDescription2EXT> VkAttributes2;
};

struct VertexFormatHash {
  auto operator()(const VertexFormat &format) const noexcept -> size_t {
    return format.GetHash();
  }
};

} // namespace Graphics