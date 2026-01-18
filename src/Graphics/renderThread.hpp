#pragma once

#include "Graphics/barrier.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "Modules/future.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <condition_variable>
#include <mutex>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

namespace Graphics::Threading {

struct RenderThreadData {
  // The usage updates recorded for all worker threads
  // We will append resource barriers after each command buffer recording
  // After reordering at the end of the frame
  std::vector<Barrier::ResourceState> usageUpdates;
  std::vector<Barrier::ResourceSync> resourceSyncs;

  VkCommandBuffer commandBuffer = nullptr;
};

struct RenderThreadInfo {
  RenderThreadData *threadData = nullptr;
  uint32_t threadIndex = 0;
  bool currentlyRecording = false;
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

extern std::mutex CanStartNewCommandsMutex;
extern bool CanStartNewCommands;
extern std::condition_variable CanStartNewCommandsCV;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

static const Type type = Type("CommandBufferRequest");

struct CommandBufferResult : Future<VkCommandBuffer>, Object {
  explicit CommandBufferResult(GraphicsContext &ctx) : context(&ctx) {}
  CommandBufferResult(const CommandBufferResult &) = delete;
  CommandBufferResult(CommandBufferResult &&) = delete;
  auto operator=(const CommandBufferResult &) -> CommandBufferResult & = delete;
  auto operator=(CommandBufferResult &&) -> CommandBufferResult & = delete;
  ~CommandBufferResult() override = default;

  GraphicsContext *context = nullptr;
  static auto GetType() -> Type const * { return &type; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return CommandBufferResult::GetType();
  }

protected:
  auto BlockUntilReady() -> Error override {
    {
      std::unique_lock<std::mutex> lock(CanStartNewCommandsMutex);
      CanStartNewCommandsCV.wait(lock,
                                 [] -> bool { return CanStartNewCommands; });
    }

    auto *cmdBuffer = VkCommandBuffer{};
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = context->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    auto result = Error::Create(
        vkAllocateCommandBuffers(context->device, &allocInfo, &cmdBuffer));
    if (result.IsError()) {
      return result;
    }

    SetData(cmdBuffer);
    return Error::Success();
  }

  auto CheckIsReady() -> Error override {
    {
      std::lock_guard<std::mutex> lock(CanStartNewCommandsMutex);
      if (!CanStartNewCommands) {
        return Error::Success();
      }
    }

    auto *cmdBuffer = VkCommandBuffer{};
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = context->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    auto result = Error::Create(
        vkAllocateCommandBuffers(context->device, &allocInfo, &cmdBuffer));
    if (result.IsError()) {
      return result;
    }

    SetData(cmdBuffer);
    return Error::Success();
  }
};

auto AquireCommandBuffer(Graphics::GraphicsContext &context,
                         RenderThreadInfo &threadInfo)
    -> Ref<CommandBufferResult>;
auto SubmitCommands(Graphics::GraphicsContext &context,
                    VkCommandBuffer commandBuffer, RenderThreadInfo &threadInfo)
    -> Error;

} // namespace Graphics::Threading