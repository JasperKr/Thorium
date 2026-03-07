#include "material.hpp"
#include "Modules/bindings.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include "lua.hpp"
#include <cstdint>
#include <imgui.h>

namespace Engine::Renderer {

const static LuaWrap::LuaEnum<AlphaMode> LuaAlphaModeEnum({
    {"opaque", AlphaMode::Opaque},
    {"mask", AlphaMode::Mask},
    {"blend", AlphaMode::Blend},
});

const static LuaWrap::LuaEnum<VkCullModeFlags> LuaCullModeEnum({
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
  binding.RegisterMember<&Material::alphaCutoff>("AlphaCutoff");
  binding.RegisterMember<&Material::shader>("Shader");

  binding.RegisterMember<&Material::preview>("Preview");
  binding.RegisterMember<&Material::albedoTexture>("AlbedoTexture");
  binding.RegisterMember<&Material::normalTexture>("NormalTexture");
  binding.RegisterMember<&Material::metallicRoughnessTexture>(
      "MetallicRoughnessTexture");
  binding.RegisterMember<&Material::ambientOcclusionTexture>(
      "AmbientOcclusionTexture");
  binding.RegisterMember<&Material::reflectanceTexture>("ReflectanceTexture");
  binding.RegisterMember<&Material::emissiveTexture>("EmissiveTexture");

  binding.RegisterMember<&Material::albedoFactor>("AlbedoFactor");
  binding.RegisterMember<&Material::roughnessFactor>("RoughnessFactor");
  binding.RegisterMember<&Material::metallicFactor>("MetallicFactor");
  binding.RegisterMember<&Material::reflectanceFactor>("ReflectanceFactor");
  binding.RegisterMember<&Material::emissiveFactor>("EmissiveFactor");

  binding.RegisterEnum<&Material::alphaMode, LuaAlphaModeEnum>("AlphaMode");
  binding.RegisterEnum<&Material::cullMode, LuaCullModeEnum>("CullMode");

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