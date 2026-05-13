#include "frustum.hpp"
#include "Modules/Math/vector.hpp"
#include "Wrap/wrap.hpp"
#include "entity.hpp"

namespace Engine {

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Frustum::IntersectsAABB(const Math::Vec3 &min, const Math::Vec3 &max,
                             bool precise) const -> bool {
  for (int i = 0; i < PlaneCount; i++) {
    int out = 0;
    auto plane = GetPlane(i);

    out += plane.Dot(Math::Vec3(min.x, min.y, min.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(max.x, min.y, min.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(min.x, max.y, min.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(max.x, max.y, min.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(min.x, min.y, max.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(max.x, min.y, max.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(min.x, max.y, max.z)) < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(max.x, max.y, max.z)) < 0.0F ? 1 : 0;

    if (out == 8) { // NOLINT
      return false;
    }
  }

  [[likely]]
  if (precise) {
    int out = 0;
    // clang-format off
    // NOLINTBEGIN
    out = 0; for (const auto &corner : Corners) { out += corner.x > max.x ? 1 : 0; }; if (out == 8) { return false; }
    out = 0; for (const auto &corner : Corners) { out += corner.x < min.x ? 1 : 0; }; if (out == 8) { return false; }
    out = 0; for (const auto &corner : Corners) { out += corner.y > max.y ? 1 : 0; }; if (out == 8) { return false; }
    out = 0; for (const auto &corner : Corners) { out += corner.y < min.y ? 1 : 0; }; if (out == 8) { return false; }
    out = 0; for (const auto &corner : Corners) { out += corner.z > max.z ? 1 : 0; }; if (out == 8) { return false; }
    out = 0; for (const auto &corner : Corners) { out += corner.z < min.z ? 1 : 0; }; if (out == 8) { return false; }
    // NOLINTEND
    // clang-format on
  }

  return true;
}

auto Frustum::FromMatrices(const Math::Matrix4x4 &viewProjectionMatrix,
                           const Math::Matrix4x4 &inverseViewProjectionMatrix)
    -> Frustum {
  Frustum frustum;

  // Define the corners of the NDC cube
  std::array<Math::Vec4, CornerCount> ndc_corners = {
      Math::Vec4(-1.0F, -1.0F, -1.0F, 1.0F), // NTL
      Math::Vec4(1.0F, -1.0F, -1.0F, 1.0F),  // NTR
      Math::Vec4(1.0F, -1.0F, 1.0F, 1.0F),   // NBR
      Math::Vec4(-1.0F, -1.0F, 1.0F, 1.0F),  // NBL
      Math::Vec4(-1.0F, 1.0F, -1.0F, 1.0F),  // FTL
      Math::Vec4(1.0F, 1.0F, -1.0F, 1.0F),   // FTR
      Math::Vec4(1.0F, 1.0F, 1.0F, 1.0F),    // FBR
      Math::Vec4(-1.0F, 1.0F, 1.0F, 1.0F)    // FBL
  };

  for (size_t i = 0; i < CornerCount; ++i) {
    Math::Vec4 world_pos = inverseViewProjectionMatrix * ndc_corners.at(i);
    frustum.Corners.at(i) = Math::Vec3{world_pos / world_pos.w};
  }

  // Gribb/Hartmann method for extracting planes from the view-projection matrix
  const auto &mat = viewProjectionMatrix;

  for (int i = 0; i < 4; ++i) {
    frustum.Left[i] = mat.At(3, i) + mat.At(0, i);
    frustum.Right[i] = mat.At(3, i) - mat.At(0, i);
    frustum.Bottom[i] = mat.At(3, i) + mat.At(1, i);
    frustum.Top[i] = mat.At(3, i) - mat.At(1, i);
    frustum.Near[i] = mat.At(3, i) + mat.At(2, i);
    frustum.Far[i] = mat.At(3, i) - mat.At(2, i);
  }

  // Normalize the planes
  frustum.Left = frustum.Left.Normalize();
  frustum.Right = frustum.Right.Normalize();
  frustum.Bottom = frustum.Bottom.Normalize();
  frustum.Top = frustum.Top.Normalize();
  frustum.Near = frustum.Near.Normalize();
  frustum.Far = frustum.Far.Normalize();

  return frustum;
}

auto Frustum::GetNearPlane(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  lua_pushnumber(state, frustum->Near.x);
  lua_pushnumber(state, frustum->Near.y);
  lua_pushnumber(state, frustum->Near.z);
  lua_pushnumber(state, frustum->Near.w);

  return 4;
};

auto Frustum::GetFarPlane(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  lua_pushnumber(state, frustum->Far.x);
  lua_pushnumber(state, frustum->Far.y);
  lua_pushnumber(state, frustum->Far.z);
  lua_pushnumber(state, frustum->Far.w);

  return 4;
};

auto Frustum::GetLeftPlane(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  lua_pushnumber(state, frustum->Left.x);
  lua_pushnumber(state, frustum->Left.y);
  lua_pushnumber(state, frustum->Left.z);
  lua_pushnumber(state, frustum->Left.w);

  return 4;
};

auto Frustum::GetRightPlane(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  lua_pushnumber(state, frustum->Right.x);
  lua_pushnumber(state, frustum->Right.y);
  lua_pushnumber(state, frustum->Right.z);
  lua_pushnumber(state, frustum->Right.w);

  return 4;
};

auto Frustum::GetTopPlane(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  lua_pushnumber(state, frustum->Top.x);
  lua_pushnumber(state, frustum->Top.y);
  lua_pushnumber(state, frustum->Top.z);
  lua_pushnumber(state, frustum->Top.w);

  return 4;
};

auto Frustum::GetBottomPlane(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  lua_pushnumber(state, frustum->Bottom.x);
  lua_pushnumber(state, frustum->Bottom.y);
  lua_pushnumber(state, frustum->Bottom.z);
  lua_pushnumber(state, frustum->Bottom.w);

  return 4;
};

auto Frustum::GetCorner(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  int index = static_cast<int>(luaL_checkinteger(state, 2));
  if (index < 0 || index >= CornerCount) {
    return luaL_error(state, "Corner index out of range");
  }

  const auto &corner = frustum->Corners.at(static_cast<size_t>(index));
  lua_pushnumber(state, corner.x);
  lua_pushnumber(state, corner.y);
  lua_pushnumber(state, corner.z);

  return 3;
};

const LuaWrap::LuaComponent FrustumComponent{{
    {"getNearPlane", Frustum::GetNearPlane},
    {"getFarPlane", Frustum::GetFarPlane},
    {"getLeftPlane", Frustum::GetLeftPlane},
    {"getRightPlane", Frustum::GetRightPlane},
    {"getTopPlane", Frustum::GetTopPlane},
    {"getBottomPlane", Frustum::GetBottomPlane},
    {"getCorner", Frustum::GetCorner},
}};

} // namespace Engine