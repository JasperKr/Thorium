#pragma once

#include "Graphics/bufferformat.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include <cstdint>
#include <flecs.h>
#include <lua.hpp>
#include <span>
namespace Engine {

constexpr size_t MaxSpotLights = 32;
static const Type spotLightType = Type("SpotLight");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::array<bool, MaxSpotLights> UsedSpotLightIndices;

struct SpotLight {
  float Range{};
  float InnerConeAngle{};
  float OuterConeAngle{};

  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  auto Write(std::span<uint8_t> buffer, flecs::entity lightEntity) const
      -> Error;

  // -1 if no free index is available
  static auto GetFreeBufferIndex() -> int32_t {
    for (int32_t i = 0; i < MaxSpotLights; ++i) {
      if (!UsedSpotLightIndices.at(i)) {
        UsedSpotLightIndices.at(i) = true;
        return i;
      }
    }
    return -1;
  }
};

struct LuaSpotLight : Object {
  explicit LuaSpotLight(const flecs::entity &entity) : entity(entity) {}

  flecs::entity entity;

  static auto Create(lua_State *state) -> int;
  static auto GetType() -> const Type * { return &spotLightType; }
  auto GetInstanceType() const -> const Type * override {
    return &spotLightType;
  }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaSpotLight> {
    return Ref<LuaSpotLight>::Make(entity);
  }
};

extern const ::LuaWrap::LuaClass SpotLightClass;

} // namespace Engine