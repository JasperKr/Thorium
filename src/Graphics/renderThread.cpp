#include "Graphics/renderThread.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "vulkan/vulkan_core.h"

namespace Graphics::Threading {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

std::mutex CanStartNewCommandsMutex{};
bool CanStartNewCommands{};
std::condition_variable CanStartNewCommandsCV{};
std::mutex ResultsMutex{};
std::vector<Ref<RenderThreadInfo>> Results{};

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto HasRenderingPermission() -> bool {
  std::lock_guard<std::mutex> lock(CanStartNewCommandsMutex);
  return CanStartNewCommands;
}

auto DemandRenderingPermission() -> void {
  std::unique_lock<std::mutex> lock(CanStartNewCommandsMutex);

  // Wait until CanStartNewCommands is true
  while (!CanStartNewCommands) {
    CanStartNewCommandsCV.wait(lock);
  }
}

auto AquireCommandBuffer(Graphics::GraphicsContext &context, AquireInfo info)
    -> Result<Ref<RenderThreadInfo>> {

  if (!HasRenderingPermission()) {
    return Error::Unexpected(
        "Cannot aquire command buffer without rendering permission");
  }

  auto threadInfo = Ref<RenderThreadInfo>::Make();
  threadInfo->currentlyRecording = true;
  threadInfo->threadData.index = info.commandIndex;
  threadInfo->threadData.queueID = info.queueID;

  {
    std::lock_guard<std::mutex> lock(ResultsMutex);
    Results.emplace_back(threadInfo);
  }

  if (context.commandPool == VK_NULL_HANDLE) {
    return Error::Unexpected(
        "Invalid command pool when aquiring command buffer");
  }

  if (context.device == VK_NULL_HANDLE) {
    return Error::Unexpected("Invalid device when aquiring command buffer");
  }

  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = context.commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;
  auto allocationResult = Error::Create(vkAllocateCommandBuffers(
      context.device, &allocInfo, &threadInfo->threadData.commandBuffer));

  if (Error::IsError(allocationResult)) {
    return allocationResult.AsUnexpected();
  }

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  auto beginResult = Error::Create(
      vkBeginCommandBuffer(threadInfo->threadData.commandBuffer, &beginInfo));

  if (Error::IsError(beginResult)) {
    return beginResult.AsUnexpected();
  }

  return threadInfo;
}

auto SubmitCommands(Graphics::GraphicsContext &context,
                    RenderThreadInfo &threadInfo) -> Error {

  auto endResult =
      Error::Create(vkEndCommandBuffer(threadInfo.threadData.commandBuffer));
  if (Error::IsError(endResult)) {
    return endResult;
  }

  {
    std::lock_guard<std::mutex> lock(threadInfo.availabilityMutex);
    threadInfo.currentlyRecording = false;
  }
  threadInfo.availabilityCV.notify_all();

  return Error::Success();
}
} // namespace Graphics::Threading