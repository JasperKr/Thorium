#pragma once

#include "Modules/error.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace Graphics {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

extern VkSemaphore globalTimelineSemaphore;
extern std::atomic<uint64_t> currentCPUTimelineValue;
extern std::condition_variable timelineCompletionCV;
extern std::mutex timelineCompletionMutex;

extern std::shared_mutex timelineSetsMutex;

extern std::unordered_set<uint64_t> uncompletedTimelineValues;
extern std::vector<uint64_t> sortedUncompletedTimelineValues;

extern std::unordered_map<VkCommandBuffer, uint64_t> pendingTimelineValues;
extern std::vector<uint64_t> sortedPendingTimelineValues;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto GetSemaphoreValue() -> uint64_t;
auto NewSemaphoreValue(VkCommandBuffer cmdBuffer) -> uint64_t;

auto IsInUse(uint64_t value) -> bool;

// Get the pending timeline semaphore values
auto GetPendingTimelineValues()
    -> std::unordered_map<VkCommandBuffer, uint64_t>;

// Set the pending timeline semaphore values
// This will overwrite the existing set, this is to reorder the values
// When command buffers are reordered before submission
auto SetPendingTimelineValues(const std::vector<uint64_t> &values) -> void;

auto UpdateSemaphoreValues(const struct GraphicsContext &context)
    -> Result<uint64_t>;
auto InitializeGlobalTimelineSemaphore(struct GraphicsContext &context)
    -> Error;
auto DeInitializeGlobalTimelineSemaphore(struct GraphicsContext &context)
    -> void;

} // namespace Graphics