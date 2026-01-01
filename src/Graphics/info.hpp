#include "Graphics/graphics.hpp"
#include <string>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

namespace Graphics::Info {
struct GpuInfo {
  uint32_t vendorID;
  uint32_t deviceID;
  std::string deviceName;
};

inline auto GetAMDShaderCorePropertiesString(VkPhysicalDevice phys)
    -> std::string {
  VkPhysicalDeviceShaderCorePropertiesAMD amdProps{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD};

  VkPhysicalDeviceProperties2 props2{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  props2.pNext = &amdProps;

  vkGetPhysicalDeviceProperties2(phys, &props2);

  std::ostringstream stream;
  stream << "VkPhysicalDeviceShaderCorePropertiesAMD\n";
  stream << "  shaderEngineCount: " << amdProps.shaderEngineCount << "\n";
  stream << "  shaderArraysPerEngineCount: "
         << amdProps.shaderArraysPerEngineCount << "\n";
  stream << "  computeUnitsPerShaderArray: "
         << amdProps.computeUnitsPerShaderArray << "\n";
  stream << "  simdPerComputeUnit: " << amdProps.simdPerComputeUnit << "\n";
  stream << "  wavefrontsPerSimd: " << amdProps.wavefrontsPerSimd << "\n";
  stream << "  wavefrontSize: " << amdProps.wavefrontSize << "\n";
  stream << "  sgprsPerSimd: " << amdProps.sgprsPerSimd << "\n";
  stream << "  minSgprAllocation: " << amdProps.minSgprAllocation << "\n";
  stream << "  maxSgprAllocation: " << amdProps.maxSgprAllocation << "\n";
  stream << "  sgprAllocationGranularity: "
         << amdProps.sgprAllocationGranularity << "\n";
  stream << "  vgprsPerSimd: " << amdProps.vgprsPerSimd << "\n";
  stream << "  minVgprAllocation: " << amdProps.minVgprAllocation << "\n";
  stream << "  maxVgprAllocation: " << amdProps.maxVgprAllocation << "\n";
  stream << "  vgprAllocationGranularity: "
         << amdProps.vgprAllocationGranularity << "\n";

  return stream.str();
}

inline auto GetGpuVendorString(uint32_t vendorID) -> std::string {
  switch (vendorID) {
  case 0x1002: // NOLINT
    return "AMD";
  case 0x10DE: // NOLINT
    return "NVIDIA";
  case 0x8086: // NOLINT
    return "Intel";
  case 0x13B5: // NOLINT
    return "ARM";
  case 0x5143: // NOLINT
    return "Qualcomm";
  default:
    return "Unknown";
  }
}

inline auto GetGpuInfo(VkPhysicalDevice phys) -> GpuInfo {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(phys, &props);

  GpuInfo info{};
  info.vendorID = props.vendorID;
  info.deviceID = props.deviceID;
  info.deviceName = std::string(static_cast<const char *>(props.deviceName));

  return info;
}

inline auto GetGpuDriverVersionString(VkPhysicalDevice phys) -> std::string {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(phys, &props);

  uint32_t major = VK_VERSION_MAJOR(props.driverVersion);
  uint32_t minor = VK_VERSION_MINOR(props.driverVersion);
  uint32_t patch = VK_VERSION_PATCH(props.driverVersion);

  std::ostringstream stream;
  stream << major << "." << minor << "." << patch;

  return stream.str();
}

inline auto GetGpuApiVersionString(VkPhysicalDevice phys) -> std::string {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(phys, &props);

  uint32_t major = VK_VERSION_MAJOR(props.apiVersion);
  uint32_t minor = VK_VERSION_MINOR(props.apiVersion);
  uint32_t patch = VK_VERSION_PATCH(props.apiVersion);

  std::ostringstream stream;
  stream << major << "." << minor << "." << patch;

  return stream.str();
}

inline auto GetGpuInfoString(VkPhysicalDevice phys) -> std::string {
  GpuInfo info = GetGpuInfo(phys);
  std::ostringstream stream;

  stream << "GPU Info:\n";
  stream << "  Device Name: " << info.deviceName << "\n";
  stream << "  Vendor: " << GetGpuVendorString(info.vendorID) << " ("
         << std::hex << info.vendorID << std::dec << ")\n";
  stream << "  Device ID: " << std::hex << info.deviceID << std::dec << "\n";
  stream << "  Driver Version: " << GetGpuDriverVersionString(phys) << "\n";
  stream << "  API Version: " << GetGpuApiVersionString(phys) << "\n";

  if (info.vendorID == 0x1002) { // NOLINT
    stream << GetAMDShaderCorePropertiesString(phys);
  }

  return stream.str();
}

} // namespace Graphics::Info