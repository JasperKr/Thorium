#define VMA_DEBUG_LOG(format, ...) printf(format, ##__VA_ARGS__)
#define VMA_VULKAN_VERSION 1004000
#define VMA_IMPLEMENTATION
#define VMA_ASSERT(expr) assert(expr)
#define VMA_IMPORT_FUNCTIONS_FROM_VOLK 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "volk/volk.h"
#include <vma/vk_mem_alloc.h>