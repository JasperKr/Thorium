#pragma once

#include <cstdint>
#include <vector>
namespace Graphics {

enum class ExtensionRequirement : uint8_t { Disabled, Optional, Required };
enum class ExtensionType : uint8_t { Instance, Device };

struct Extension {
  const char *name;
  ExtensionType type;
  ExtensionRequirement requirement;
};

struct DeviceSettings {
  // Optional hardware raytracing support,
  // requires enabling specific Vulkan extensions
  // And enables parts of the engine api to support raytracing features
  ExtensionRequirement hardwareRaytracing = ExtensionRequirement::Disabled;

  // Allows for raytracing features to be used inline with rasterization and compute,
  // without needing to separate them into different render passes and command buffers.
  ExtensionRequirement inlineRaytracing = ExtensionRequirement::Disabled;
  std::vector<Extension> requiredExtensions;
};
} // namespace Graphics