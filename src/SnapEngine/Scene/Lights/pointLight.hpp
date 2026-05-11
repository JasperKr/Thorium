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

constexpr size_t MaxPointLights = 512;
static const Type pointLightType = Type("PointLight");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::array<bool, MaxPointLights> UsedPointLightIndices;

struct PointLight {
  float Range{};

  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  auto Write(std::span<uint8_t> buffer, flecs::entity lightEntity) const
      -> Error;

  // -1 if no free index is available
  static auto GetFreeBufferIndex() -> int32_t {
    for (int32_t i = 0; i < MaxPointLights; ++i) {
      if (!UsedPointLightIndices.at(i)) {
        UsedPointLightIndices.at(i) = true;
        return i;
      }
    }
    return -1;
  }
};

struct LuaPointLight : Object {
  explicit LuaPointLight(const flecs::entity &entity) : entity(entity) {}

  flecs::entity entity;

  static auto Create(lua_State *state) -> int;
  static auto GetType() -> const Type * { return &pointLightType; }
  auto GetInstanceType() const -> const Type * override {
    return &pointLightType;
  }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaPointLight> {
    return Ref<LuaPointLight>::Make(entity);
  }
};

extern const LuaWrap::LuaClass PointLightClass;

} // namespace Engine