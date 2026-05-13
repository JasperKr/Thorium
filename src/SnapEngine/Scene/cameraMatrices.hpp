#pragma once

#include "Modules/Math/matrix.hpp"
#include "Wrap/wrap.hpp"
namespace Engine {

struct CameraMatrices {
  Math::Matrix4x4 RotationMatrix;
  Math::Matrix4x4 InverseRotationMatrix;
  Math::Matrix4x4 ViewMatrix;
  Math::Matrix4x4 InverseViewMatrix;
  Math::Matrix4x4 ProjectionMatrix;
  Math::Matrix4x4 InverseProjectionMatrix;
  Math::Matrix4x4 ViewProjectionMatrix;
  Math::Matrix4x4 InverseViewProjectionMatrix;
  Math::Matrix4x4 RotationProjectionMatrix;
  Math::Matrix4x4 InverseRotationProjectionMatrix;

  [[nodiscard]] auto GetFrustum() const -> struct Frustum;
  auto Update() -> void;

  static auto GetRotationMatrix(lua_State *state) -> int;
  static auto GetInverseRotationMatrix(lua_State *state) -> int;
  static auto GetViewMatrix(lua_State *state) -> int;
  static auto GetInverseViewMatrix(lua_State *state) -> int;
  static auto GetProjectionMatrix(lua_State *state) -> int;
  static auto GetInverseProjectionMatrix(lua_State *state) -> int;
  static auto GetViewProjectionMatrix(lua_State *state) -> int;
  static auto GetInverseViewProjectionMatrix(lua_State *state) -> int;
  static auto GetRotationProjectionMatrix(lua_State *state) -> int;
  static auto GetInverseRotationProjectionMatrix(lua_State *state) -> int;
};

extern const LuaWrap::LuaComponent CameraMatricesComponent;

} // namespace Engine