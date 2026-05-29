#include "transform.hpp"
#include "Modules/Math/eulerAngle.hpp"
#include "Modules/Math/math.hpp"
#include "Modules/Math/mathTypes.hpp"
#include "Modules/Math/matrix.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <format>
#include <imgui.h>
#include <lua.h>
#include <lua.hpp>

namespace Engine {
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

auto LuaTransform::SetPosition(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
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

  transform->SetPosition(luaL_checkscalar(state, 2), luaL_checkscalar(state, 3),
                         luaL_checkscalar(state, 4));

  return 0;
}

auto LuaTransform::GetPosition(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto position = transform->GetPosition();
  lua_pushnumber(state, position.x);
  lua_pushnumber(state, position.y);
  lua_pushnumber(state, position.z);
  return 3;
}

auto LuaTransform::SetRotation(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
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

  transform->SetRotation(luaL_checkscalar(state, 2), luaL_checkscalar(state, 3),
                         luaL_checkscalar(state, 4),
                         luaL_checkscalar(state, 5));

  return 0;
}

auto LuaTransform::GetRotation(lua_State *state) -> int {
  // Todo: Type is not Entity, but Lua<...>, LuaCamera for example, which contains an entity field.
  // Need a way to abstract this, so we can get the entity regardless of the specific Lua wrapper type.
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto rotation = transform->GetRotation();
  lua_pushnumber(state, rotation.x);
  lua_pushnumber(state, rotation.y);
  lua_pushnumber(state, rotation.z);
  lua_pushnumber(state, rotation.w);

  return 4;
}

auto LuaTransform::SetScale(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
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

  transform->SetScale(luaL_checkscalar(state, 2), luaL_checkscalar(state, 3),
                      luaL_checkscalar(state, 4));

  return 0;
}

auto LuaTransform::GetScale(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto scale = transform->GetScale();
  lua_pushnumber(state, scale.x);
  lua_pushnumber(state, scale.y);
  lua_pushnumber(state, scale.z);

  return 3;
}

auto LuaTransform::SetTransform(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
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

  transform->SetPosition(luaL_checkscalar(state, 2), luaL_checkscalar(state, 3),
                         luaL_checkscalar(state, 4));

  transform->SetRotation(luaL_checkscalar(state, 5), luaL_checkscalar(state, 6),
                         luaL_checkscalar(state, 7),

                         luaL_checkscalar(state, 8));
  transform->SetScale(luaL_checkscalar(state, 9), luaL_checkscalar(state, 10),
                      luaL_checkscalar(state, 11));

  return 0;
}

auto LuaTransform::GetTransform(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto position = transform->GetPosition();
  auto rotation = transform->GetRotation();
  auto scale = transform->GetScale();

  lua_pushnumber(state, position.x);
  lua_pushnumber(state, position.y);
  lua_pushnumber(state, position.z);

  lua_pushnumber(state, rotation.x);
  lua_pushnumber(state, rotation.y);
  lua_pushnumber(state, rotation.z);
  lua_pushnumber(state, rotation.w);

  lua_pushnumber(state, scale.x);
  lua_pushnumber(state, scale.y);
  lua_pushnumber(state, scale.z);

  return 10;
}

auto LuaTransform::GetLocalMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
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

auto LuaTransform::GetWorldMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
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

  NormalMatrix = Math::Matrix3x3(WorldMatrix).InverseTranspose();

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

auto LuaTransform::GetUp(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto vec = transform->GetRotation().RotateVector({0.0F, 1.0F, 0.0F});
  lua_pushnumber(state, vec.x);
  lua_pushnumber(state, vec.y);
  lua_pushnumber(state, vec.z);

  return 3;
}

auto LuaTransform::GetRight(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto vec = transform->GetRotation().RotateVector({1.0F, 0.0F, 0.0F});
  lua_pushnumber(state, vec.x);
  lua_pushnumber(state, vec.y);
  lua_pushnumber(state, vec.z);

  return 3;
}

auto LuaTransform::GetForward(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto vec = transform->GetRotation().RotateVector({0.0F, 0.0F, 1.0F});
  lua_pushnumber(state, vec.x);
  lua_pushnumber(state, vec.y);
  lua_pushnumber(state, vec.z);

  return 3;
}

auto LuaTransform::GetInverseUp(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto vec =
      transform->GetRotation().Inverse().RotateVector({0.0F, 1.0F, 0.0F});
  lua_pushnumber(state, vec.x);
  lua_pushnumber(state, vec.y);
  lua_pushnumber(state, vec.z);

  return 3;
}

auto LuaTransform::GetInverseRight(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto vec =
      transform->GetRotation().Inverse().RotateVector({1.0F, 0.0F, 0.0F});
  lua_pushnumber(state, vec.x);
  lua_pushnumber(state, vec.y);
  lua_pushnumber(state, vec.z);

  return 3;
}

auto LuaTransform::GetInverseForward(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua<LuaTransform>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto vec =
      transform->GetRotation().Inverse().RotateVector({0.0F, 0.0F, 1.0F});
  lua_pushnumber(state, vec.x);
  lua_pushnumber(state, vec.y);
  lua_pushnumber(state, vec.z);

  return 3;
}

extern const ::LuaWrap::LuaComponent TransformComponent{{
    {"setPosition", LuaTransform::SetPosition},
    {"getPosition", LuaTransform::GetPosition},
    {"setRotation", LuaTransform::SetRotation},
    {"getRotation", LuaTransform::GetRotation},
    {"setScale", LuaTransform::SetScale},
    {"getScale", LuaTransform::GetScale},
    {"setTransform", LuaTransform::SetTransform},
    {"getTransform", LuaTransform::GetTransform},
    {"getLocalMatrix", LuaTransform::GetLocalMatrix},
    {"getWorldMatrix", LuaTransform::GetWorldMatrix},
    {"getUp", LuaTransform::GetUp},
    {"getRight", LuaTransform::GetRight},
    {"getForward", LuaTransform::GetForward},
    {"getInverseUp", LuaTransform::GetInverseUp},
    {"getInverseRight", LuaTransform::GetInverseRight},
    {"getInverseForward", LuaTransform::GetInverseForward},
}};

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
} // namespace Engine