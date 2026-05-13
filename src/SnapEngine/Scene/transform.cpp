#include "transform.hpp"
#include "Modules/Math/eulerAngle.hpp"
#include "Modules/Math/math.hpp"
#include "Modules/Math/mathTypes.hpp"
#include "Modules/Math/matrix.hpp"
#include "Wrap/wrap.hpp"
#include "entity.hpp"
#include <format>
#include <imgui.h>
#include <lua.hpp>

namespace Engine {
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

auto Transform::SetPosition(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 4) {
    return luaL_error(state, "Expected 3 arguments for position (x, y, z)");
  }

  auto *transform = entity->try_get_mut<Transform>();
  if (transform == nullptr) {
    entity->add<Transform>();
    transform = entity->try_get_mut<Transform>();
  }

  transform->Position.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  transform->Position.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  transform->Position.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));

  return 0;
}

auto Transform::GetPosition(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushnumber(state, transform->Position.x);
  lua_pushnumber(state, transform->Position.y);
  lua_pushnumber(state, transform->Position.z);
  return 3;
}

auto Transform::SetRotation(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 5) {
    return luaL_error(state, "Expected 4 arguments for position (x, y, z, w)");
  }

  auto *transform = entity->try_get_mut<Transform>();
  if (transform == nullptr) {
    entity->add<Transform>();
    transform = entity->try_get_mut<Transform>();
  }

  transform->Rotation.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  transform->Rotation.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  transform->Rotation.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));
  transform->Rotation.w = static_cast<Math::Scalar>(luaL_checknumber(state, 5));

  return 0;
}

auto Transform::GetRotation(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushnumber(state, transform->Rotation.x);
  lua_pushnumber(state, transform->Rotation.y);
  lua_pushnumber(state, transform->Rotation.z);
  lua_pushnumber(state, transform->Rotation.w);
  return 4;
}

auto Transform::SetScale(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 4) {
    return luaL_error(state, "Expected 3 arguments for position (x, y, z)");
  }

  auto *transform = entity->try_get_mut<Transform>();
  if (transform == nullptr) {
    entity->add<Transform>();
    transform = entity->try_get_mut<Transform>();
  }

  transform->Scale.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  transform->Scale.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  transform->Scale.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));

  return 0;
}

auto Transform::GetScale(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushnumber(state, transform->Scale.x);
  lua_pushnumber(state, transform->Scale.y);
  lua_pushnumber(state, transform->Scale.z);

  return 3;
}

auto Transform::SetTransform(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 11) {
    return luaL_error(state, "Expected 10 arguments for transform (x, y, z, "
                             "rotX, rotY, rotZ, rotW, scaleX, scaleY, scaleZ)");
  }

  auto *transform = entity->try_get_mut<Transform>();
  if (transform == nullptr) {
    entity->add<Transform>();
    transform = entity->try_get_mut<Transform>();
  }

  transform->Position.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  transform->Position.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  transform->Position.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));

  transform->Rotation.x = static_cast<Math::Scalar>(luaL_checknumber(state, 5));
  transform->Rotation.y = static_cast<Math::Scalar>(luaL_checknumber(state, 6));
  transform->Rotation.z = static_cast<Math::Scalar>(luaL_checknumber(state, 7));
  transform->Rotation.w = static_cast<Math::Scalar>(luaL_checknumber(state, 8));

  transform->Scale.x = static_cast<Math::Scalar>(luaL_checknumber(state, 9));
  transform->Scale.y = static_cast<Math::Scalar>(luaL_checknumber(state, 10));
  transform->Scale.z = static_cast<Math::Scalar>(luaL_checknumber(state, 11));

  return 0;
}

auto Transform::GetTransform(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushnumber(state, transform->Position.x);
  lua_pushnumber(state, transform->Position.y);
  lua_pushnumber(state, transform->Position.z);

  lua_pushnumber(state, transform->Rotation.x);
  lua_pushnumber(state, transform->Rotation.y);
  lua_pushnumber(state, transform->Rotation.z);
  lua_pushnumber(state, transform->Rotation.w);

  lua_pushnumber(state, transform->Scale.x);
  lua_pushnumber(state, transform->Scale.y);
  lua_pushnumber(state, transform->Scale.z);

  return 10;
}

auto Transform::GetLocalMatrix(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  return transform->GetLocalMatrix().ToLua(state);
}

auto Transform::GetWorldMatrix(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  return transform->GetWorldMatrix().ToLua(state);
}

auto Transform::UpdateLocalMatrix() -> void {
  if (!LocalDirty) {
    return;
  }

  LocalMatrix =
      Math::Matrix4x4::TransformationMatrix(Position, Scale, Rotation);
  LocalDirty = false;
  WorldDirty = true;
}

auto Transform::UpdateWorldMatrix(const Transform *parent) -> void {
  if (!WorldDirty) {
    return;
  }

  if (parent != nullptr) {
    WorldMatrix = parent->WorldMatrix * LocalMatrix;
  } else {
    WorldMatrix = LocalMatrix;
  }

  // WorldDirty = false;
}

auto Transform::DrawGUI() -> void {
  ImGuiDataType dataType = sizeof(Math::Scalar) == sizeof(double)
                               ? ImGuiDataType_Double
                               : ImGuiDataType_Float;
  if (ImGui::DragScalarN("Position", dataType, (void *)Position.Ptr(), 3,
                         0.1F)) {
    LocalDirty = true;
  }

  auto eulerRotation = Math::Conversions::ToEuler(Rotation);
  if (ImGui::DragScalarN("Rotation (x, y, z)", dataType,
                         (void *)eulerRotation.Ptr(), 3, 0.01F)) {
    Rotation = Math::Conversions::ToQuaternion(eulerRotation);
    LocalDirty = true;
  }

  if (ImGui::DragScalarN("Scale", dataType, (void *)Scale.Ptr(), 3, 0.1F)) {
    LocalDirty = true;
  }

  ImGui::Text("%s",
              std::format("Local Matrix:\n{}", LocalMatrix.ToString()).c_str());
  ImGui::Text("%s",
              std::format("World Matrix:\n{}", WorldMatrix.ToString()).c_str());
}

extern const LuaWrap::LuaComponent TransformComponent{{
    {"setPosition", Transform::SetPosition},
    {"getPosition", Transform::GetPosition},
    {"setRotation", Transform::SetRotation},
    {"getRotation", Transform::GetRotation},
    {"setScale", Transform::SetScale},
    {"getScale", Transform::GetScale},
    {"setTransform", Transform::SetTransform},
    {"getTransform", Transform::GetTransform},
    {"getLocalMatrix", Transform::GetLocalMatrix},
    {"getWorldMatrix", Transform::GetWorldMatrix},
}};

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
} // namespace Engine