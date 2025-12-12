#include "../Modules/console.hpp"

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintLibrary(std::string_view format, Args &&...args) {
  if (CurrentLogLevel <= LogLevel::Debug) {
    std::cout << ColorText("[LIBRARY] ", ConsoleColor::Blue)
              << std::vformat(format, std::make_format_args(args...)) << '\n';
  }
}

#define VMA_DEBUG_LOG(format, ...) PrintLibrary(format, ##__VA_ARGS__)
#define VMA_VULKAN_VERSION 1004000
#define VMA_IMPLEMENTATION
#define VMA_ASSERT(expr) assert(expr)
#define VMA_IMPORT_FUNCTIONS_FROM_VOLK 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "volk/volk.h"
#include <vma/vk_mem_alloc.h>