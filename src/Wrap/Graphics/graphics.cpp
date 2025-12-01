#include "Graphics/graphics.hpp"
#include "Graphics/render.hpp"
#include "Graphics/rendertarget.hpp"
#include "Graphics/shader.hpp"
#include "Modules/object.hpp"
#include "Wrap/wrap.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Graphics {
auto wrap_Present(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto result = Present(*ctx);

  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.ToString().c_str());
  }

  return 0;
}

// RenderTarget functions
auto wrap_Push(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  RenderTarget::Push(*ctx);
  return 0;
}
auto wrap_Pop(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto error = RenderTarget::Pop(*ctx);
  if (Error::IsError(error)) {
    return luaL_error(state, "%s", error.ToString().c_str());
  }

  return 0;
}
auto wrap_Reset(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  RenderTarget::Reset(*ctx);
  return 0;
}

// Rendertarget state functions

// Options: "never", "less", "equal", "lequal", "greater", "notequal", "gequal", "always", writeEnable: bool
auto wrap_SetDepthMode(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  // Enable is set based on the compare op
  const char *compareOpStr = luaL_checkstring(state, 1);
  bool writeEnable = lua_toboolean(state, 2) != 0;
  VkCompareOp compareOp = VK_COMPARE_OP_MAX_ENUM;

  if (strcmp(compareOpStr, "never") == 0) {
    compareOp = VK_COMPARE_OP_NEVER;
  } else if (strcmp(compareOpStr, "less") == 0) {
    compareOp = VK_COMPARE_OP_LESS;
  } else if (strcmp(compareOpStr, "equal") == 0) {
    compareOp = VK_COMPARE_OP_EQUAL;
  } else if (strcmp(compareOpStr, "lequal") == 0) {
    compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  } else if (strcmp(compareOpStr, "greater") == 0) {
    compareOp = VK_COMPARE_OP_GREATER;
  } else if (strcmp(compareOpStr, "notequal") == 0) {
    compareOp = VK_COMPARE_OP_NOT_EQUAL;
  } else if (strcmp(compareOpStr, "gequal") == 0) {
    compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
  } else if (strcmp(compareOpStr, "always") == 0) {
    compareOp = VK_COMPARE_OP_ALWAYS;
  } else {
    return luaL_error(state, "Invalid depth compare operation: %s",
                      compareOpStr);
  }

  auto enableRead =
      compareOp != VK_COMPARE_OP_ALWAYS && compareOp != VK_COMPARE_OP_NEVER;

  RenderTarget::SetDepthMode(enableRead, writeEnable, compareOp);
  return 0;
}

// Options: none, front, back, always
auto wrap_SetCullMode(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();

  const char *cullModeStr = luaL_checkstring(state, 1);
  VkCullModeFlags cullMode = VK_CULL_MODE_FLAG_BITS_MAX_ENUM;

  if (strcmp(cullModeStr, "none") == 0) {
    cullMode = VK_CULL_MODE_NONE;
  } else if (strcmp(cullModeStr, "front") == 0) {
    cullMode = VK_CULL_MODE_FRONT_BIT;
  } else if (strcmp(cullModeStr, "back") == 0) {
    cullMode = VK_CULL_MODE_BACK_BIT;
  } else if (strcmp(cullModeStr, "always") == 0) {
    cullMode = VK_CULL_MODE_FRONT_AND_BACK;
  } else {
    return luaL_error(state, "Invalid cull mode: %s", cullModeStr);
  }

  RenderTarget::SetCullMode(cullMode);
  return 0;
}

// Options: fill, line, point
auto wrap_SetPolygonMode(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();

  const char *polygonModeStr = luaL_checkstring(state, 1);
  VkPolygonMode polygonMode = VK_POLYGON_MODE_MAX_ENUM;

  if (strcmp(polygonModeStr, "fill") == 0) {
    polygonMode = VK_POLYGON_MODE_FILL;
  } else if (strcmp(polygonModeStr, "line") == 0) {
    polygonMode = VK_POLYGON_MODE_LINE;
  } else if (strcmp(polygonModeStr, "point") == 0) {
    polygonMode = VK_POLYGON_MODE_POINT;
  } else {
    return luaL_error(state, "Invalid polygon mode: %s", polygonModeStr);
  }

  RenderTarget::SetPolygonMode(polygonMode);
  return 0;
}
auto wrap_SetViewport(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  VkViewport viewport{};
  viewport.x = static_cast<float>(luaL_checknumber(state, 1));
  viewport.y = static_cast<float>(luaL_checknumber(state, 2));
  viewport.width = static_cast<float>(luaL_checknumber(state, 3));
  viewport.height = static_cast<float>(luaL_checknumber(state, 4));
  viewport.minDepth =
      static_cast<float>(luaL_optnumber(state, 5, 0.0)); // NOLINT
  viewport.maxDepth =
      static_cast<float>(luaL_optnumber(state, 6, 1.0)); // NOLINT
  RenderTarget::SetViewport(viewport);
  return 0;
}
auto wrap_SetScissor(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  VkRect2D scissor{};
  scissor.offset.x = static_cast<int32_t>(luaL_checkinteger(state, 1));
  scissor.offset.y = static_cast<int32_t>(luaL_checkinteger(state, 2));
  scissor.extent.width = static_cast<uint32_t>(luaL_checkinteger(state, 3));
  scissor.extent.height = static_cast<uint32_t>(luaL_checkinteger(state, 4));
  RenderTarget::SetScissor(scissor);
  return 0;
}
auto wrap_ClipScissor(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  VkRect2D scissor{};
  scissor.offset.x = static_cast<int32_t>(luaL_checkinteger(state, 1));
  scissor.offset.y = static_cast<int32_t>(luaL_checkinteger(state, 2));
  scissor.extent.width = static_cast<uint32_t>(luaL_checkinteger(state, 3));
  scissor.extent.height = static_cast<uint32_t>(luaL_checkinteger(state, 4));
  RenderTarget::ClipScissor(scissor);
  return 0;
}
auto wrap_SetShader(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto *shaderHandle =
      LuaWrap::FromLuaObject<Graphics::Shader::ShaderModule>(state, 1);
  RenderTarget::SetShader(Ref<Shader::ShaderModule>(shaderHandle));
  return 0;
}
auto wrap_SetRenderTargets(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  std::vector<Ref<RenderTarget::RenderTarget>> renderTargets;

  luaL_checktype(state, 1, LUA_TTABLE);
  size_t len = lua_objlen(state, 1);

  for (size_t i = 0; i < len; ++i) {
    auto *renderTarget = LuaWrap::FromLuaObject<RenderTarget::RenderTarget>(
        state, static_cast<int>(i + 1));

    renderTargets.emplace_back(renderTarget);
    lua_pop(state, 1);
  }

  RenderTarget::SetRenderTargets(renderTargets);
  return 0;
}

auto wrap_SetLineWidth(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto lineWidth = static_cast<float>(luaL_checknumber(state, 1));
  if (lineWidth <= 0.0F) {
    return luaL_error(state, "Line width must be greater than 0.");
  }
  RenderTarget::SetLineWidth(lineWidth);
  return 0;
}

// Options: ccw, cw
auto wrap_SetWindingOrder(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  VkFrontFace frontFace = VK_FRONT_FACE_MAX_ENUM;

  const char *order = luaL_checkstring(state, 1);
  if (strcmp(order, "cw") == 0) {
    frontFace = VK_FRONT_FACE_CLOCKWISE;
  } else if (strcmp(order, "ccw") == 0) {
    frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  } else {
    return luaL_error(state, "Invalid winding order: %s", order);
  }

  RenderTarget::SetWindingOrder(frontFace);
  return 0;
}

// Getters

// Options: "never", "less", "equal", "lequal", "greater", "notequal", "gequal", "always", writeEnable: bool
auto wrap_GetDepthMode(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  bool enableRead = false;
  bool writeEnable = false;
  VkCompareOp compareOp = VK_COMPARE_OP_MAX_ENUM;
  std::tie(enableRead, writeEnable, compareOp) = RenderTarget::GetDepthMode();

  const char *compareOpStr = "unknown";
  switch (compareOp) {
  case VK_COMPARE_OP_NEVER:
    compareOpStr = "never";
    break;
  case VK_COMPARE_OP_LESS:
    compareOpStr = "less";
    break;
  case VK_COMPARE_OP_EQUAL:
    compareOpStr = "equal";
    break;
  case VK_COMPARE_OP_LESS_OR_EQUAL:
    compareOpStr = "lequal";
    break;
  case VK_COMPARE_OP_GREATER:
    compareOpStr = "greater";
    break;
  case VK_COMPARE_OP_NOT_EQUAL:
    compareOpStr = "notequal";
    break;
  case VK_COMPARE_OP_GREATER_OR_EQUAL:
    compareOpStr = "gequal";
    break;
  case VK_COMPARE_OP_ALWAYS:
    compareOpStr = "always";
    break;
  default:
    break;
  }

  lua_pushstring(state, compareOpStr);
  lua_pushboolean(state, static_cast<int>(writeEnable));
  return 2;
}

// Options: none, front, back, always
auto wrap_GetCullMode(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto cullMode = RenderTarget::GetCullMode();

  const char *cullModeStr = "unknown";
  switch (cullMode) {
  case VK_CULL_MODE_NONE:
    cullModeStr = "none";
    break;
  case VK_CULL_MODE_FRONT_BIT:
    cullModeStr = "front";
    break;
  case VK_CULL_MODE_BACK_BIT:
    cullModeStr = "back";
    break;
  case VK_CULL_MODE_FRONT_AND_BACK:
    cullModeStr = "always";
    break;
  default:
    break;
  }

  lua_pushstring(state, cullModeStr);
  return 1;
}

// Options: fill, line, point
auto wrap_GetPolygonMode(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto polygonMode = RenderTarget::GetPolygonMode();

  const char *polygonModeStr = "unknown";
  switch (polygonMode) {
  case VK_POLYGON_MODE_FILL:
    polygonModeStr = "fill";
    break;
  case VK_POLYGON_MODE_LINE:
    polygonModeStr = "line";
    break;
  case VK_POLYGON_MODE_POINT:
    polygonModeStr = "point";
    break;
  default:
    break;
  }

  lua_pushstring(state, polygonModeStr);
  return 1;
}

// Options: x, y, width, height, minDepth, maxDepth
auto wrap_GetViewport(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto viewport = RenderTarget::GetViewport();

  lua_pushnumber(state, static_cast<lua_Number>(viewport.x));
  lua_pushnumber(state, static_cast<lua_Number>(viewport.y));
  lua_pushnumber(state, static_cast<lua_Number>(viewport.width));
  lua_pushnumber(state, static_cast<lua_Number>(viewport.height));
  lua_pushnumber(state, static_cast<lua_Number>(viewport.minDepth));
  lua_pushnumber(state, static_cast<lua_Number>(viewport.maxDepth));
  return 6; // NOLINT
}

// Options: offsetX, offsetY, extentWidth, extentHeight
auto wrap_GetScissor(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto scissor = RenderTarget::GetScissor();

  lua_pushinteger(state, static_cast<lua_Integer>(scissor.offset.x));
  lua_pushinteger(state, static_cast<lua_Integer>(scissor.offset.y));
  lua_pushinteger(state, static_cast<lua_Integer>(scissor.extent.width));
  lua_pushinteger(state, static_cast<lua_Integer>(scissor.extent.height));
  return 4;
}

// Returns shader handle as integer
auto wrap_GetShader(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto ref = RenderTarget::GetShader();

  auto type = *Shader::ShaderModule::GetType();

  LuaWrap::PushLuaType(state, type, ref.get());

  return 1;
}

auto wrap_GetRenderTargets(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto renderTargets = RenderTarget::GetRenderTargets();

  lua_newtable(state);
  for (size_t i = 0; i < renderTargets.size(); ++i) {
    auto &renderTarget = renderTargets[i];
    auto type = *RenderTarget::RenderTarget::GetType();

    LuaWrap::PushLuaType(state, type, renderTarget.get());
    lua_rawseti(state, -2, static_cast<int>(i + 1));
  }

  return 1;
}
auto wrap_GetLineWidth(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto lineWidth = RenderTarget::GetLineWidth();
  lua_pushnumber(state, static_cast<lua_Number>(lineWidth));
  return 1;
}
auto wrap_GetWindingOrder(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto frontFace = RenderTarget::GetWindingOrder();

  const char *order = "unknown";
  if (frontFace == VK_FRONT_FACE_CLOCKWISE) {
    order = "cw";
  } else if (frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE) {
    order = "ccw";
  }

  lua_pushstring(state, order);
  return 1;
}

} // namespace Graphics