#include "material.hpp"
#include "Modules/bindings.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include "lua.hpp"
#include <cstdint>
#include <imgui.h>

namespace Engine::Renderer {

const static LuaWrap::LuaEnum<AlphaMode>
    LuaAlphaModeEnum("AlphaMode", {
                                      {"opaque", AlphaMode::Opaque},
                                      {"mask", AlphaMode::Mask},
                                      {"blend", AlphaMode::Blend},
                                  });

const static LuaWrap::LuaEnum<VkCullModeFlags>
    LuaCullModeEnum("CullMode", {
                                    {"none", VK_CULL_MODE_NONE},
                                    {"front", VK_CULL_MODE_FRONT_BIT},
                                    {"back", VK_CULL_MODE_BACK_BIT},
                                    {"all", VK_CULL_MODE_FRONT_AND_BACK},
                                });

auto Material::LoadBinding(lua_State *state) -> int {
  const std::vector<std::pair<std::string, lua_CFunction>> methods = {
      {"setTextureUVIndex", wrap_setTextureUVIndex},
      {"getTextureUVIndex", wrap_getTextureUVIndex},
  };

  auto binding = Bindings::LuaBoundStruct<Material>("Material");
  binding.RegisterMember<&Material::name>("Name");
  binding.RegisterMember<&Material::alphaCutoff>(
      "AlphaCutoff", "Alpha cutoff value used when AlphaMode is Mask.\n"
                     "Pixels with alpha below this value will be discarded.");
  binding.RegisterMember<&Material::shader>(
      "Shader", "Shader module used for rendering this material.");

  binding.RegisterMember<&Material::preview>(
      "Preview", "Preview texture for this material, used in the editor.");
  binding.RegisterMember<&Material::albedoTexture>(
      "AlbedoTexture", "srgb RGBA texture for base color.");
  binding.RegisterMember<&Material::normalTexture>(
      "NormalTexture", "Linear RGB texture for normals.");
  binding.RegisterMember<&Material::metallicRoughnessTexture>(
      "MetallicRoughnessTexture",
      "Linear RG texture where R is metallic and G is roughness.");
  binding.RegisterMember<&Material::ambientOcclusionTexture>(
      "AmbientOcclusionTexture", "Linear R texture for ambient occlusion.");
  binding.RegisterMember<&Material::reflectanceTexture>(
      "ReflectanceTexture", "Linear R texture for reflectance.");
  binding.RegisterMember<&Material::emissiveTexture>(
      "EmissiveTexture", "Linear RGB texture for emissive color.");

  binding.RegisterMember<&Material::albedoFactor>(
      "AlbedoFactor",
      "Albedo color factor multiplied with the albedo texture.");
  binding.RegisterMember<&Material::roughnessFactor>(
      "RoughnessFactor",
      "Roughness factor multiplied with the roughness texture.");
  binding.RegisterMember<&Material::metallicFactor>(
      "MetallicFactor",
      "Metallic factor multiplied with the metallic texture.");
  binding.RegisterMember<&Material::reflectanceFactor>("ReflectanceFactor");
  binding.RegisterMember<&Material::emissiveFactor>(
      "EmissiveFactor",
      "Emissive color factor multiplied with the emissive texture.");

  binding.RegisterEnum<&Material::alphaMode, LuaAlphaModeEnum>(
      "AlphaMode", "Alpha mode used for rendering this material.");
  binding.RegisterEnum<&Material::cullMode, LuaCullModeEnum>(
      "CullMode", "Culling mode used for rendering this material.");

  binding.Register(state, methods);

  return 1;
}

auto Material::DrawUiElement() -> Error {
  ImGui::Text("Material: %s", name.c_str());

  auto width =
      ImGui::GetContentRegionAvail().x - (ImGui::GetStyle().ItemSpacing.x * 2);
  auto texWidth = preview.isValid() ? preview.get()->GetWidth() : 0;

  if (preview.isValid()) {
    ImGui::Image(preview.get(), ImVec2());
  }

  return Error::Success();
}

auto Material::wrap_setTextureUVIndex(lua_State *state) -> int {
  auto *material = LuaWrap::ObjectFromLua<Material>(state, 1);
  if (material == nullptr) {
    return luaL_error(state, "Invalid Material object");
  }

  auto textureType = luaL_checkinteger(state, 2);
  if (textureType < 0 ||
      textureType >= static_cast<int>(material->textureUVIndices.size())) {
    return luaL_error(state, "Invalid texture type index");
  }

  auto uvIndex = luaL_checkinteger(state, 3);
  if (uvIndex < 0 || uvIndex > UINT8_MAX) {
    return luaL_error(state, "UV index must be between 0 and UINT8_MAX");
  }

  material->textureUVIndices.at(textureType) = static_cast<uint8_t>(uvIndex);
  return 0;
}

auto Material::wrap_getTextureUVIndex(lua_State *state) -> int {
  auto *material = LuaWrap::ObjectFromLua<Material>(state, 1);
  if (material == nullptr) {
    return luaL_error(state, "Invalid Material object");
  }

  auto textureType = luaL_checkinteger(state, 2);
  if (textureType < 0 ||
      textureType >= static_cast<int>(material->textureUVIndices.size())) {
    return luaL_error(state, "Invalid texture type index");
  }

  lua_pushinteger(state, material->textureUVIndices.at(textureType));

  return 1;
}

} // namespace Engine::Renderer