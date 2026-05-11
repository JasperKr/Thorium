#pragma once

#include "Graphics/bufferformat.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include <cstdint>
#include <flecs.h>
#include <lua.hpp>

namespace Engine {

constexpr size_t MaxRectangleLights = 32;
static const Type rectangleLightType = Type("RectangleLight");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::array<bool, MaxRectangleLights> UsedRectangleLightIndices;

struct RectangleLight {
  Math::Vec2 Size{1.0F, 1.0F};
  float Range{};

  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  auto Write(std::span<uint8_t> buffer, flecs::entity lightEntity) const
      -> Error;

  // -1 if no free index is available
  static auto GetFreeBufferIndex() -> int32_t {
    for (int32_t i = 0; i < MaxRectangleLights; ++i) {
      if (!UsedRectangleLightIndices.at(i)) {
        UsedRectangleLightIndices.at(i) = true;
        return i;
      }
    }
    return -1;
  }
};

struct LuaRectangleLight : Object {
  explicit LuaRectangleLight(const flecs::entity &entity) : entity(entity) {}

  flecs::entity entity;

  static auto Create(lua_State *state) -> int;
  static auto GetType() -> const Type * { return &rectangleLightType; }
  auto GetInstanceType() const -> const Type * override {
    return &rectangleLightType;
  }

  static auto FromEntity(const flecs::entity &entity)
      -> Ref<LuaRectangleLight> {
    return Ref<LuaRectangleLight>::Make(entity);
  }
};

extern const LuaWrap::LuaClass RectangleLightClass;

} // namespace Engine