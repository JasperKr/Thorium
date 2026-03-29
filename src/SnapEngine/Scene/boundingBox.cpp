#include "boundingBox.hpp"
#include "Wrap/wrap.hpp"
#include <lua.h>

namespace Engine {
auto BoundingBox::GetCenter() const -> Math::Vec3 {
  return (Min + Max) * 0.5F; // NOLINT
}

auto BoundingBox::GetSize() const -> Math::Vec3 { return Max - Min; }

auto BoundingBox::Union(const BoundingBox &other) const -> BoundingBox {
  BoundingBox result{};
  result.Min = Math::Min(Min, other.Min);
  result.Max = Math::Max(Max, other.Max);
  return result;
}

auto BoundingBox::Union(const BoundingBox &other, BoundingBox &result) const
    -> void {
  result.Min = Math::Min(Min, other.Min);
  result.Max = Math::Max(Max, other.Max);
}

auto BoundingBox::UnionInPlace(const BoundingBox &other) -> void {
  Min = Math::Min(Min, other.Min);
  Max = Math::Max(Max, other.Max);
}

auto BoundingBox::Intersect(const BoundingBox &other) const -> BoundingBox {
  BoundingBox result{};
  result.Min = Math::Max(Min, other.Min);
  result.Max = Math::Min(Max, other.Max);
  return result;
}

auto BoundingBox::Intersect(const BoundingBox &other, BoundingBox &result) const
    -> void {
  result.Min = Math::Max(Min, other.Min);
  result.Max = Math::Min(Max, other.Max);
}

auto BoundingBox::IntersectInPlace(const BoundingBox &other) -> void {
  Min = Math::Max(Min, other.Min);
  Max = Math::Min(Max, other.Max);
}

auto BoundingBox::IsValid() const -> bool {
  return Min.x <= Max.x && Min.y <= Max.y && Min.z <= Max.z;
}

auto LuaBoundingBox::GetMin(lua_State *state) -> int {
  auto *luaBoundingBox = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 1);

  if (luaBoundingBox == nullptr) {
    return luaL_error(state, "Expected a BoundingBox object");
  }

  auto boundingBox = luaBoundingBox->entity.get_ref<BoundingBox>();
  if (boundingBox.get() == nullptr) {
    return luaL_error(state, "BoundingBox component not found");
  }

  lua_pushnumber(state, boundingBox->Min.x);
  lua_pushnumber(state, boundingBox->Min.y);
  lua_pushnumber(state, boundingBox->Min.z);

  return 3;
}

auto LuaBoundingBox::SetMin(lua_State *state) -> int {
  auto *luaBoundingBox = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 1);

  if (luaBoundingBox == nullptr) {
    return luaL_error(state, "Expected a BoundingBox object");
  }

  auto boundingBox = luaBoundingBox->entity.get_ref<BoundingBox>();
  if (boundingBox.get() == nullptr) {
    return luaL_error(state, "BoundingBox component not found");
  }

  boundingBox->Min.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  boundingBox->Min.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  boundingBox->Min.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));

  return 0;
}

auto LuaBoundingBox::GetMax(lua_State *state) -> int {
  auto *luaBoundingBox = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 1);

  if (luaBoundingBox == nullptr) {
    return luaL_error(state, "Expected a BoundingBox object");
  }

  auto boundingBox = luaBoundingBox->entity.get_ref<BoundingBox>();
  if (boundingBox.get() == nullptr) {
    return luaL_error(state, "BoundingBox component not found");
  }

  lua_pushnumber(state, boundingBox->Max.x);
  lua_pushnumber(state, boundingBox->Max.y);
  lua_pushnumber(state, boundingBox->Max.z);

  return 3;
}

auto LuaBoundingBox::SetMax(lua_State *state) -> int {
  auto *luaBoundingBox = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 1);

  if (luaBoundingBox == nullptr) {
    return luaL_error(state, "Expected a BoundingBox object");
  }

  auto boundingBox = luaBoundingBox->entity.get_ref<BoundingBox>();
  if (boundingBox.get() == nullptr) {
    return luaL_error(state, "BoundingBox component not found");
  }

  boundingBox->Max.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  boundingBox->Max.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  boundingBox->Max.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));

  return 0;
}

auto LuaBoundingBox::GetCenter(lua_State *state) -> int {
  auto *luaBoundingBox = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 1);

  if (luaBoundingBox == nullptr) {
    return luaL_error(state, "Expected a BoundingBox object");
  }

  auto boundingBox = luaBoundingBox->entity.get_ref<BoundingBox>();
  if (boundingBox.get() == nullptr) {
    return luaL_error(state, "BoundingBox component not found");
  }

  auto center = boundingBox->GetCenter();
  lua_pushnumber(state, center.x);
  lua_pushnumber(state, center.y);
  lua_pushnumber(state, center.z);

  return 3;
}

auto LuaBoundingBox::GetSize(lua_State *state) -> int {
  auto *luaBoundingBox = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 1);

  if (luaBoundingBox == nullptr) {
    return luaL_error(state, "Expected a BoundingBox object");
  }

  auto boundingBox = luaBoundingBox->entity.get_ref<BoundingBox>();
  if (boundingBox.get() == nullptr) {
    return luaL_error(state, "BoundingBox component not found");
  }

  auto size = boundingBox->GetSize();
  lua_pushnumber(state, size.x);
  lua_pushnumber(state, size.y);
  lua_pushnumber(state, size.z);

  return 3;
}

auto LuaBoundingBox::Union(lua_State *state) -> int {
  auto *luaBoundingBox1 = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 1);
  auto *luaBoundingBox2 = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 2);

  if (luaBoundingBox1 == nullptr || luaBoundingBox2 == nullptr) {
    return luaL_error(state, "Expected two BoundingBox objects");
  }

  auto boundingBox1 = luaBoundingBox1->entity.get_ref<BoundingBox>();
  auto boundingBox2 = luaBoundingBox2->entity.get_ref<BoundingBox>();

  if (boundingBox1.get() == nullptr || boundingBox2.get() == nullptr) {
    return luaL_error(state,
                      "BoundingBox component not found in one of the objects");
  }

  auto *luaBoundingBoxOut = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 3);
  if (luaBoundingBoxOut == nullptr) {
    auto result = boundingBox1->Union(*boundingBox2.get());

    // Create a new LuaBoundingBox for the result
    auto resultEntity = luaBoundingBox1->entity.world().entity();
    resultEntity.set<BoundingBox>(result);
    auto resultLuaBoundingBox = LuaBoundingBox(resultEntity);
    LuaWrap::PushObject(state, LuaBoundingBox::GetType(),
                        &resultLuaBoundingBox);
    return 1;
  }

  auto boundingBoxOut = luaBoundingBoxOut->entity.get_ref<BoundingBox>();
  if (boundingBoxOut.get() == nullptr) {
    return luaL_error(state,
                      "BoundingBox component not found in output object");
  }

  boundingBox1->Union(*boundingBox2.get(), *boundingBoxOut.get());

  LuaWrap::PushObject(state, LuaBoundingBox::GetType(), luaBoundingBoxOut);

  return 1;
}

auto LuaBoundingBox::Intersect(lua_State *state) -> int {
  auto *luaBoundingBox1 = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 1);
  auto *luaBoundingBox2 = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 2);

  if (luaBoundingBox1 == nullptr || luaBoundingBox2 == nullptr) {
    return luaL_error(state, "Expected two BoundingBox objects");
  }

  auto boundingBox1 = luaBoundingBox1->entity.get_ref<BoundingBox>();
  auto boundingBox2 = luaBoundingBox2->entity.get_ref<BoundingBox>();

  if (boundingBox1.get() == nullptr || boundingBox2.get() == nullptr) {
    return luaL_error(state,
                      "BoundingBox component not found in one of the objects");
  }

  auto *luaBoundingBoxOut = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 3);

  if (luaBoundingBoxOut == nullptr) {
    auto result = boundingBox1->Intersect(*boundingBox2.get());

    // Create a new LuaBoundingBox for the result
    auto resultEntity = luaBoundingBox1->entity.world().entity();
    resultEntity.set<BoundingBox>(result);
    auto resultLuaBoundingBox = LuaBoundingBox(resultEntity);
    LuaWrap::PushObject(state, LuaBoundingBox::GetType(),
                        &resultLuaBoundingBox);
    return 1;
  }

  auto boundingBoxOut = luaBoundingBoxOut->entity.get_ref<BoundingBox>();
  if (boundingBoxOut.get() == nullptr) {
    return luaL_error(state,
                      "BoundingBox component not found in output object");
  }

  boundingBox1->Intersect(*boundingBox2.get(), *boundingBoxOut.get());
  LuaWrap::PushObject(state, LuaBoundingBox::GetType(), luaBoundingBoxOut);

  return 1;
}

auto LuaBoundingBox::UnionInPlace(lua_State *state) -> int {
  auto *luaBoundingBox1 = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 1);
  auto *luaBoundingBox2 = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 2);

  if (luaBoundingBox1 == nullptr || luaBoundingBox2 == nullptr) {
    return luaL_error(state, "Expected two BoundingBox objects");
  }

  auto boundingBox1 = luaBoundingBox1->entity.get_ref<BoundingBox>();
  auto boundingBox2 = luaBoundingBox2->entity.get_ref<BoundingBox>();

  if (boundingBox1.get() == nullptr || boundingBox2.get() == nullptr) {
    return luaL_error(state,
                      "BoundingBox component not found in one of the objects");
  }

  boundingBox1->UnionInPlace(*boundingBox2.get());
  return 0;
}

auto LuaBoundingBox::IntersectInPlace(lua_State *state) -> int {
  auto *luaBoundingBox1 = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 1);
  auto *luaBoundingBox2 = LuaWrap::ObjectFromLua<LuaBoundingBox>(state, 2);

  if (luaBoundingBox1 == nullptr || luaBoundingBox2 == nullptr) {
    return luaL_error(state, "Expected two BoundingBox objects");
  }

  auto boundingBox1 = luaBoundingBox1->entity.get_ref<BoundingBox>();
  auto boundingBox2 = luaBoundingBox2->entity.get_ref<BoundingBox>();

  if (boundingBox1.get() == nullptr || boundingBox2.get() == nullptr) {
    return luaL_error(state,
                      "BoundingBox component not found in one of the objects");
  }

  boundingBox1->IntersectInPlace(*boundingBox2.get());
  return 0;
}

const LuaWrap::LuaClass BoundingBoxClass = {
    .Name = "BoundingBox",
    .Type = LuaBoundingBox::GetType(),
    .Methods = {
        {"getMin", LuaBoundingBox::GetMin},
        {"setMin", LuaBoundingBox::SetMin},
        {"getMax", LuaBoundingBox::GetMax},
        {"setMax", LuaBoundingBox::SetMax},
        {"getCenter", LuaBoundingBox::GetCenter},
        {"getSize", LuaBoundingBox::GetSize},
        {"union", LuaBoundingBox::Union},
        {"intersect", LuaBoundingBox::Intersect},
        {"unionInPlace", LuaBoundingBox::UnionInPlace},
        {"intersectInPlace", LuaBoundingBox::IntersectInPlace},
    }};

} // namespace Engine