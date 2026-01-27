#include "Graphics/semaphoreManager.hpp"
#include "Modules/console.hpp"
#include "vulkan/vulkan_core.h"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>
#include <vector>

namespace Graphics {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

VkSemaphore globalTimelineSemaphore{};
std::atomic<uint64_t> currentCPUTimelineValue{};

std::shared_mutex timelineSetsMutex{};
std::unordered_set<uint64_t> uncompletedTimelineValues{};
std::vector<uint64_t> sortedUncompletedTimelineValues{};
std::unordered_map<VkCommandBuffer, uint64_t> pendingTimelineValues{};
std::unordered_map<uint64_t, VkCommandBuffer> pendingTimelineValueInv{};
std::vector<uint64_t> sortedPendingTimelineValues{};

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto GetSemaphoreValue() -> uint64_t { return currentCPUTimelineValue.load(); }

auto NewSemaphoreValue(VkCommandBuffer cmdBuffer) -> uint64_t {
  // Does not have to be an increasing value, just unique
  auto value = currentCPUTimelineValue.fetch_add(1) + 1;
  {
    std::unique_lock lock(timelineSetsMutex);
    pendingTimelineValues.emplace(cmdBuffer, value);
    pendingTimelineValueInv.emplace(value, cmdBuffer);
  }
  return value;
}

auto IsInUse(uint64_t value) -> bool {
  std::shared_lock lock(timelineSetsMutex);
  return uncompletedTimelineValues.contains(value) ||
         pendingTimelineValueInv.contains(value);
}

auto GetPendingTimelineValues()
    -> std::unordered_map<VkCommandBuffer, uint64_t> {
  std::shared_lock lock(timelineSetsMutex);
  return pendingTimelineValues;
}

auto SetPendingTimelineValues(const std::vector<uint64_t> &values) -> void {
  std::unique_lock lock(timelineSetsMutex);
  sortedPendingTimelineValues = values;
}

// Update the semaphore values before submitting command buffers
// And returns the latest queued timeline value for signalling with a semaphore
auto UpdateSemaphoreValues(const GraphicsContext &context) -> Result<uint64_t> {

  std::lock_guard lock(timelineSetsMutex);

  if (sortedPendingTimelineValues.empty()) {
    PrintWarning("No sorting pending timeline values to update");

    for (const auto &pair : pendingTimelineValues) {
      sortedUncompletedTimelineValues.emplace_back(pair.second);
    }

    pendingTimelineValues.clear();
  } else {
    sortedUncompletedTimelineValues.insert(
        sortedUncompletedTimelineValues.end(),
        sortedPendingTimelineValues.begin(), sortedPendingTimelineValues.end());

    // Remove only the values that were added
    for (const auto &pair : sortedPendingTimelineValues) {
      auto iter = pendingTimelineValues.find(pendingTimelineValueInv.at(pair));
      if (iter != pendingTimelineValues.end()) {
        pendingTimelineValues.erase(iter);
      }
      pendingTimelineValueInv.erase(pair);
    }
  }

  if (sortedUncompletedTimelineValues.empty()) {
    PrintWarning("No uncompleted timeline values after update");
    return GetSemaphoreValue();
  }

  for (const auto &value : sortedUncompletedTimelineValues) {
    uncompletedTimelineValues.insert(value);
  }

  auto latestValue = sortedUncompletedTimelineValues.back();

  sortedPendingTimelineValues.clear();

  uint64_t completedValue = UINT64_MAX;

  {
    std::lock_guard lock(Graphics::GraphicsContext::mutexes.device);
    auto result = Error::Create(vkGetSemaphoreCounterValue(
        context.device, globalTimelineSemaphore, &completedValue));

    if (Error::IsError(result)) {
      return result.AsUnexpected();
    }
  }

  // Find the index of the completed value
  // This will have been ordered so all values less than or equal to the completed value
  // Will be considered completed and can be removed from the uncompleted set
  auto newStart = -1;

  for (size_t i = 0; i < sortedUncompletedTimelineValues.size(); i++) {
    if (sortedUncompletedTimelineValues[i] == completedValue) {
      newStart = static_cast<int>(i) + 1; // +1 to move past completed value
      break;
    }
  }

  if (newStart == -1) {
    // No values completed
    return latestValue;
  }

  // Remove completed values from the uncompleted set
  for (int i = 0; i < newStart; i++) {
    uncompletedTimelineValues.erase(sortedUncompletedTimelineValues[i]);
  }

  // Cut off completed values from the sorted vector
  sortedUncompletedTimelineValues.erase(
      sortedUncompletedTimelineValues.begin(),
      sortedUncompletedTimelineValues.begin() + newStart);

  return latestValue;
}

auto InitializeGlobalTimelineSemaphore(GraphicsContext &context) -> Error {
  VkSemaphoreTypeCreateInfo timelineInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = 0,
  };

  VkSemaphoreCreateInfo semInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &timelineInfo,
  };

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    auto result = Error::Create(vkCreateSemaphore(
        context.device, &semInfo, nullptr, &globalTimelineSemaphore));
    if (Error::IsError(result)) {
      return result;
    }
  }

  return Error::Success();
}

auto DeInitializeGlobalTimelineSemaphore(GraphicsContext &context) -> void {
  if (globalTimelineSemaphore != VK_NULL_HANDLE) {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    vkDestroySemaphore(context.device, globalTimelineSemaphore, nullptr);
    globalTimelineSemaphore = VK_NULL_HANDLE;
  }
}

} // namespace Graphics