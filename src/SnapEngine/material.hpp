#pragma once

#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/object.hpp"
#include <array>
#include <string>

#include "flecs.h"
#include "lua.hpp"

#include <vulkan/vulkan_core.h>
namespace Engine::Renderer {

enum class AlphaMode : uint8_t {
  Opaque = 0,
  Mask = 1,
  Blend = 2,
};

using TexRef = Ref<Graphics::Texture>;

const Type materialType = Type("Material");

struct Material {
  Material(std::string name, Ref<Graphics::Shader::ShaderModule> shader,
           TexRef preview, TexRef albedoTexture, TexRef normalTexture,
           TexRef metallicRoughnessTexture, TexRef ambientOcclusionTexture,
           TexRef reflectanceTexture, TexRef emissiveTexture)
      : name(std::move(name)), shader(std::move(shader)),
        preview(std::move(preview)), albedoTexture(std::move(albedoTexture)),
        normalTexture(std::move(normalTexture)),
        metallicRoughnessTexture(std::move(metallicRoughnessTexture)),
        ambientOcclusionTexture(std::move(ambientOcclusionTexture)),
        reflectanceTexture(std::move(reflectanceTexture)),
        emissiveTexture(std::move(emissiveTexture)) {}

  std::string name;

  VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
  AlphaMode alphaMode = AlphaMode::Opaque;
  float alphaCutoff = 0.5F; // NOLINT
  Ref<Graphics::Shader::ShaderModule> shader;

  TexRef preview;

  TexRef albedoTexture;            // Linear RGBA
  TexRef normalTexture;            // Linear RGB
  TexRef metallicRoughnessTexture; // Linear RG
  TexRef ambientOcclusionTexture;  // Linear R
  TexRef reflectanceTexture;       // Linear R
  TexRef emissiveTexture;          // Linear RGB

  Math::Vec4 albedoFactor = Math::Vec4(1.0F, 1.0F, 1.0F, 1.0F);
  float roughnessFactor = 1.0F;
  float metallicFactor = 1.0F;
  float reflectanceFactor = 1.0F;
  Math::Vec3 emissiveFactor = Math::Vec3(0.0F, 0.0F, 0.0F);

  // which UV set each texture uses in the order of:
  // 0: albedo
  // 1: normal
  // 2: roughness
  // 3: metallic
  // 4: ao
  // 5: reflectance
  // 6: emissive
  // NOLINTNEXTLINE (magic numbers)
  std::array<uint8_t, 7> textureUVIndices = {0};

  static auto wrap_setCullMode(lua_State *state) -> int;
  static auto wrap_getCullMode(lua_State *state) -> int;

  static auto wrap_setAlphaMode(lua_State *state) -> int;
  static auto wrap_getAlphaMode(lua_State *state) -> int;

  static auto wrap_setTextureUVIndex(lua_State *state) -> int;
  static auto wrap_getTextureUVIndex(lua_State *state) -> int;
};

struct LuaMaterial : Object {
  explicit LuaMaterial(Material *material) : material(material) {}

  Material *material;
  flecs::entity entity;

  static auto GetType() -> const Type * { return &materialType; }
  auto GetInstanceType() const -> const Type * override {
    return &materialType;
  }

  static auto Create(lua_State *state) -> int;
};

} // namespace Engine::Renderer