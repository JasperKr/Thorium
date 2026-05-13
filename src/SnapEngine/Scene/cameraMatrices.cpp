#include "cameraMatrices.hpp"
#include "Wrap/wrap.hpp"
#include "entity.hpp"
#include "frustum.hpp"

namespace Engine {

auto CameraMatrices::GetFrustum() const -> Frustum {
  return Frustum::FromMatrices(ViewProjectionMatrix,
                               InverseViewProjectionMatrix);
}

auto CameraMatrices::Update() -> void {
  ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
  InverseViewProjectionMatrix = ViewProjectionMatrix.Inverse();
  RotationProjectionMatrix = ProjectionMatrix * RotationMatrix;
  InverseRotationProjectionMatrix = RotationProjectionMatrix.Inverse();
}

auto CameraMatrices::GetRotationMatrix(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->RotationMatrix.ToLua(state);
}

auto CameraMatrices::GetInverseRotationMatrix(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->InverseRotationMatrix.ToLua(state);
}

auto CameraMatrices::GetViewMatrix(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->ViewMatrix.ToLua(state);
}

auto CameraMatrices::GetInverseViewMatrix(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->InverseViewMatrix.ToLua(state);
}

auto CameraMatrices::GetProjectionMatrix(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->ProjectionMatrix.ToLua(state);
}

auto CameraMatrices::GetInverseProjectionMatrix(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->InverseProjectionMatrix.ToLua(state);
}

auto CameraMatrices::GetViewProjectionMatrix(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->ViewProjectionMatrix.ToLua(state);
}

auto CameraMatrices::GetInverseViewProjectionMatrix(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->InverseViewProjectionMatrix.ToLua(state);
}

auto CameraMatrices::GetRotationProjectionMatrix(lua_State *state) -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->RotationProjectionMatrix.ToLua(state);
}

auto CameraMatrices::GetInverseRotationProjectionMatrix(lua_State *state)
    -> int {
  auto *entity = LuaWrap::ObjectFromLua<Entity>(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->InverseRotationProjectionMatrix.ToLua(state);
}

const LuaWrap::LuaComponent CameraMatricesComponent{{
    {"getRotationMatrix", CameraMatrices::GetRotationMatrix},
    {"getInverseRotationMatrix", CameraMatrices::GetInverseRotationMatrix},
    {"getViewMatrix", CameraMatrices::GetViewMatrix},
    {"getInverseViewMatrix", CameraMatrices::GetInverseViewMatrix},
    {"getProjectionMatrix", CameraMatrices::GetProjectionMatrix},
    {"getInverseProjectionMatrix", CameraMatrices::GetInverseProjectionMatrix},
    {"getViewProjectionMatrix", CameraMatrices::GetViewProjectionMatrix},
    {"getInverseViewProjectionMatrix",
     CameraMatrices::GetInverseViewProjectionMatrix},
    {"getRotationProjectionMatrix",
     CameraMatrices::GetRotationProjectionMatrix},
    {"getInverseRotationProjectionMatrix",
     CameraMatrices::GetInverseRotationProjectionMatrix},
}};

} // namespace Engine