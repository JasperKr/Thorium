#include "utils.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <unistd.h>

namespace Utils {

auto InterleaveSpans(
    std::vector<std::pair<std::span<const uint8_t>, size_t>> &spans)
    -> std::vector<uint8_t> {
  assert(!spans.empty());
  size_t count = spans.size();

  size_t elementsPerSpan = 0;

  // compute total output size
  size_t totalSize = 0;
  for (const auto &[span, elemSize] : spans) {
    totalSize += span.size();
    if (elementsPerSpan == 0) {
      elementsPerSpan = span.size() / elemSize;
    } else {
      assert(elementsPerSpan == (span.size() / elemSize));
    }
  }

  std::vector<uint8_t> output(totalSize);

  size_t offset = 0;
  for (size_t i = 0; i < elementsPerSpan; ++i) {
    for (const auto &[span, size] : spans) {
      std::memcpy(&output[offset], span.data() + (i * size), size); // NOLINT
      offset += size;
    }
  }

  return output;
}

auto AlignUp(size_t value, size_t alignment) -> size_t {
  assert(alignment != 0 && (alignment & (alignment - 1)) == 0 &&
         "Alignment must be a non-zero power of two.");
  return (value + alignment - 1) & ~(alignment - 1);
}

auto AlignDown(size_t value, size_t alignment) -> size_t {
  assert(alignment != 0 && (alignment & (alignment - 1)) == 0 &&
         "Alignment must be a non-zero power of two.");
  return value & ~(alignment - 1);
}

auto Subspan(std::span<const uint8_t> span, size_t offset, size_t size)
    -> std::span<const uint8_t> {
  auto range = std::min(offset + size, span.size());
  return span.subspan(offset, range - offset);
}

auto Subspan(std::vector<uint8_t> &vec, size_t offset, size_t size)
    -> std::span<uint8_t> {
  auto range = std::min(offset + size, vec.size());
  // NOLINTNEXTLINE, because of pointer arithmetic
  return {vec.data() + offset, range - offset};
}

auto SetBindingToSlot(uint32_t set, uint32_t binding) -> uint64_t {
  return (static_cast<uint64_t>(set) << 32U) | binding; // NOLINT
}

auto SlotToSetBinding(uint64_t slot) -> std::pair<uint32_t, uint32_t> {
  auto set = static_cast<uint32_t>((slot >> 32U) & UINT32_MAX); // NOLINT
  auto binding = static_cast<uint32_t>(slot & UINT32_MAX);
  return {set, binding};
}

#if defined(__linux__)
auto GetMemoryUsage() -> size_t {
  // Read memory usage from /proc/self/statm
  FILE *file = fopen("/proc/self/statm", "r"); // NOLINT
  if (file == nullptr) {
    return 0; // Could not open file
  }

  size_t residentPages = 0;
  if (fscanf(file, "%*s %zu", &residentPages) != 1) {
    fclose(file); // NOLINT
    return 0;     // Could not read resident pages
  }
  fclose(file); // NOLINT

  // Get the system page size
  long pageSize = sysconf(_SC_PAGESIZE);
  if (pageSize <= 0) {
    return 0; // Could not get page size
  }

  return residentPages * static_cast<size_t>(pageSize);
}
#elif defined(_WIN32)
auto GetMemoryUsage() -> size_t {
  // Windows implementation using GetProcessMemoryInfo
  PROCESS_MEMORY_COUNTERS memCounters;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &memCounters,
                           sizeof(memCounters))) {
    return memCounters.WorkingSetSize; // Resident memory usage in bytes
  }
  return 0; // Could not get memory info
}
#elif defined(__APPLE__)
auto GetMemoryUsage() -> size_t {
  // macOS implementation using task_info
  struct mach_task_basic_info info;
  mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info,
                &infoCount) == KERN_SUCCESS) {
    return info.resident_size; // Resident memory usage in bytes
  }
  return 0; // Could not get memory info
}
#else
auto GetMemoryUsage() -> size_t {
  return 0; // Unsupported platform
}
#endif

} // namespace Utils