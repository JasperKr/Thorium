#include "framegraph.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/resourceUsage.hpp"
#include "Modules/error.hpp"
#include <variant>
#include <vulkan/vulkan_core.h>

namespace Graphics {

auto FrameGraph::Submit(const VirtualCommandBuffer &commands) -> Error {
  commandBuffer = commands;

  CHECK_ERR(Compile());

  return {};
}

auto FrameGraph::ValidateGraph() -> Error {
  int startEndState{};

  for (const auto &command : commandBuffer.commands) {
    if (command.GetType() == CommandType::vkCmdBeginRendering) {
      startEndState++;
    } else if (command.GetType() == CommandType::vkCmdEndRendering) {
      startEndState--;

      if (startEndState < 0) {
        return Error::Create("Ended rendering more than rendering was begun.");
      }
    }
  }

  if (startEndState != 0) {
    return Error::Create("Begin / End count mismatch");
  }

  return {};
}

auto FrameGraph::GetResourceStateAt(VkBuffer buffer, uint64_t time)
    -> Result<ResourceState> {
  auto updates = commandBuffer.bufferStateUpdates[buffer];

  auto iter = std::ranges::upper_bound(updates, time, std::less{},
                                       &decltype(updates)::value_type::second);

  return iter == updates.begin() ? ResourceState{} : std::prev(iter)->first;
}

auto FrameGraph::GetResourceStateAt(VkImageView image, uint64_t time)
    -> Result<ResourceState> {
  auto updates = commandBuffer.imageStateUpdates[image];

  auto iter = std::ranges::upper_bound(updates, time, std::less{},
                                       &decltype(updates)::value_type::second);

  return iter == updates.begin() ? ResourceState{} : std::prev(iter)->first;
}

auto FrameGraph::BuildGraph() -> Error {
  for (const auto &command : commandBuffer.commands) {
  }
}

auto FrameGraph::Compile() -> Error {
  CHECK_ERR(ValidateGraph());

  return {};
}

auto FrameGraph::Write(VkCommandBuffer cmdBuffer) -> Error {
  for (const auto &command : commandBuffer.commands) {
    std::visit(
        [cmdBuffer](const auto &cmdData) -> void { cmdData.Call(cmdBuffer); },
        command.data);
  }

  return {};
}

} // namespace Graphics