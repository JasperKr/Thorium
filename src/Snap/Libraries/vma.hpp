#pragma once

#include <volk/volk.h>

#define VMA_IMPORT_FUNCTIONS_FROM_VOLK 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_VULKAN_VERSION 1004000
#define VMA_ASSERT(expr) assert(expr)

#include <cassert>
#include <vma/vk_mem_alloc.h>