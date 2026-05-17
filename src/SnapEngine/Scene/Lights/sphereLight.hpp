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

constexpr size_t MaxSphereLights = 32;
static const Type sphereLightType = Type("SphereLight");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::array<bool, MaxSphereLights> UsedSphereLightIndices;

struct SphereLight {
  float Range{};
  float Radius{1.0F};

  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  auto Write(std::span<uint8_t> buffer, flecs::entity lightEntity) const
      -> Error;

  // -1 if no free index is available
  static auto GetFreeBufferIndex() -> int32_t {
    for (int32_t i = 0; i < MaxSphereLights; ++i) {
      if (!UsedSphereLightIndices.at(i)) {
        UsedSphereLightIndices.at(i) = true;
        return i;
      }
    }
    return -1;
  }
};

struct LuaSphereLight : Object {
  explicit LuaSphereLight(const flecs::entity &entity) : entity(entity) {}

  flecs::entity entity;

  static auto Create(lua_State *state) -> int;
  static auto GetType() -> const Type * { return &sphereLightType; }
  auto GetInstanceType() const -> const Type * override {
    return &sphereLightType;
  }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaSphereLight> {
    return Ref<LuaSphereLight>::Make(entity);
  }
};

extern const ::LuaWrap::LuaClass SphereLightClass;

} // namespace Engine