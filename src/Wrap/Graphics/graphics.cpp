#include "Graphics/graphics.hpp"
#include "Graphics/render.hpp"
#include "Graphics/rendertarget.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/object.hpp"
#include "Wrap/wrap.hpp"
#include "vulkan/vulkan_core.h"
#include <cstring>
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

auto inline ConvertStringToBlendOp(const char *string) -> VkBlendOp {
  // add, sub, revsub, min, max

  if (strcmp(string, "add") == 0) {
    return VK_BLEND_OP_ADD;
  }
  if (strcmp(string, "sub") == 0) {
    return VK_BLEND_OP_SUBTRACT;
  }
  if (strcmp(string, "revsub") == 0) {
    return VK_BLEND_OP_REVERSE_SUBTRACT;
  }
  if (strcmp(string, "min") == 0) {
    return VK_BLEND_OP_MIN;
  }
  if (strcmp(string, "max") == 0) {
    return VK_BLEND_OP_MAX;
  }
  {
    return VK_BLEND_OP_ADD; // Default
  }
}

auto inline ConvertStringToBlendFactor(const char *string) -> VkBlendFactor {
  // zero, one, srccolor, oneminussrccolor, dstcolor, oneminusdstcolor,
  // srcalpha, oneminussrcalpha, dstalpha, oneminusdstalpha, constantcolor,
  // oneminusconstantcolor, constantalpha, oneminusconstantalpha, srcalphasat

  if (strcmp(string, "zero") == 0) {
    return VK_BLEND_FACTOR_ZERO;
  }
  if (strcmp(string, "one") == 0) {
    return VK_BLEND_FACTOR_ONE;
  }
  if (strcmp(string, "srccolor") == 0) {
    return VK_BLEND_FACTOR_SRC_COLOR;
  }
  if (strcmp(string, "oneminussrccolor") == 0) {
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
  }
  if (strcmp(string, "dstcolor") == 0) {
    return VK_BLEND_FACTOR_DST_COLOR;
  }
  if (strcmp(string, "oneminusdstcolor") == 0) {
    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  }
  if (strcmp(string, "srcalpha") == 0) {
    return VK_BLEND_FACTOR_SRC_ALPHA;
  }
  if (strcmp(string, "oneminussrcalpha") == 0) {
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  }
  if (strcmp(string, "dstalpha") == 0) {
    return VK_BLEND_FACTOR_DST_ALPHA;
  }
  if (strcmp(string, "oneminusdstalpha") == 0) {
    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  }
  if (strcmp(string, "constantcolor") == 0) {
    return VK_BLEND_FACTOR_CONSTANT_COLOR;
  }
  if (strcmp(string, "oneminusconstantcolor") == 0) {
    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
  }
  if (strcmp(string, "constantalpha") == 0) {
    return VK_BLEND_FACTOR_CONSTANT_ALPHA;
  }
  if (strcmp(string, "oneminusconstantalpha") == 0) {
    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
  }
  if (strcmp(string, "srcalphasat") == 0) {
    return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
  }

  {
    return VK_BLEND_FACTOR_ONE; // Default
  }
}

// blendmode: { blendmode = "none"|"alpha"|"add"|"sub"|"mul", alphamode = "alphamultiply"|"premultiplied", mask = int }
// Or, { srccolor = "", dstcolor = "", srcalpha = "", dstalpha = "", opcolor = "", opalpha = "", mask = int }
auto inline FromLuaState(lua_State *state)
    -> VkPipelineColorBlendAttachmentState {
  // blend mode from lua top of stack
  VkPipelineColorBlendAttachmentState blendMode = {};

  // Enable detailed blendmode if idx 1 is a string
  if (lua_isstring(state, 1) != 0) {
    // Error if idx 2 is not a string
    if (lua_isstring(state, 2) == 0) {
      luaL_error(state, "Expected string as second argument for blendmode");
    }

    const char *modeStr = luaL_checkstring(state, 1);
    if (strcmp(modeStr, "none") == 0) {
      blendMode.blendEnable = VK_FALSE;
    } else if (strcmp(modeStr, "alpha") == 0) {
      blendMode.blendEnable = VK_TRUE;
      blendMode.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      blendMode.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      blendMode.colorBlendOp = VK_BLEND_OP_ADD;
      blendMode.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      blendMode.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      blendMode.alphaBlendOp = VK_BLEND_OP_ADD;
    } else if (strcmp(modeStr, "add") == 0) {
      blendMode.blendEnable = VK_TRUE;
      blendMode.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
      blendMode.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
      blendMode.colorBlendOp = VK_BLEND_OP_ADD;
      blendMode.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      blendMode.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      blendMode.alphaBlendOp = VK_BLEND_OP_ADD;
    } else if (strcmp(modeStr, "sub") == 0) {
      blendMode.blendEnable = VK_TRUE;
      blendMode.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
      blendMode.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
      blendMode.colorBlendOp = VK_BLEND_OP_SUBTRACT;
      blendMode.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      blendMode.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      blendMode.alphaBlendOp = VK_BLEND_OP_SUBTRACT;
    } else if (strcmp(modeStr, "mul") == 0) {
      blendMode.blendEnable = VK_TRUE;
      blendMode.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
      blendMode.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
      blendMode.colorBlendOp = VK_BLEND_OP_ADD;
      blendMode.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
      blendMode.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      blendMode.alphaBlendOp = VK_BLEND_OP_ADD;
    } else {
      luaL_error(state, "Invalid blend mode: %s", modeStr);
    }

    const char *alphaModeStr = luaL_checkstring(state, 2);
    if (strcmp(alphaModeStr, "alphamultiply") == 0) {
      // No change needed
    } else if (strcmp(alphaModeStr, "premultiplied") == 0) {
      blendMode.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    } else {
      luaL_error(state, "Invalid alpha blend mode: %s", alphaModeStr);
    }
  } else {
    // Detailed blendmode
    lua_getfield(state, 1, "srccolor");
    const char *srcColorStr = luaL_checkstring(state, -1);
    lua_pop(state, 1);

    lua_getfield(state, 1, "dstcolor");
    const char *dstColorStr = luaL_checkstring(state, -1);
    lua_pop(state, 1);

    lua_getfield(state, 1, "srcalpha");
    const char *srcAlphaStr = luaL_checkstring(state, -1);
    lua_pop(state, 1);

    lua_getfield(state, 1, "dstalpha");
    const char *dstAlphaStr = luaL_checkstring(state, -1);
    lua_pop(state, 1);

    lua_getfield(state, 1, "opcolor");
    const char *opColorStr = luaL_checkstring(state, -1);
    lua_pop(state, 1);

    lua_getfield(state, 1, "opalpha");
    const char *opAlphaStr = luaL_checkstring(state, -1);
    lua_pop(state, 1);

    blendMode.blendEnable = VK_TRUE;
    blendMode.srcColorBlendFactor = ConvertStringToBlendFactor(srcColorStr);
    blendMode.dstColorBlendFactor = ConvertStringToBlendFactor(dstColorStr);
    blendMode.colorBlendOp = ConvertStringToBlendOp(opColorStr);
    blendMode.srcAlphaBlendFactor = ConvertStringToBlendFactor(srcAlphaStr);
    blendMode.dstAlphaBlendFactor = ConvertStringToBlendFactor(dstAlphaStr);
    blendMode.alphaBlendOp = ConvertStringToBlendOp(opAlphaStr);
  }

  // color write mask
  lua_getfield(state, 1, "mask");
  if (lua_isnumber(state, -1) != 0) {
    blendMode.colorWriteMask = static_cast<uint32_t>(lua_tointeger(state, -1));
  } else {
    blendMode.colorWriteMask = static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) |
                               static_cast<uint32_t>(VK_COLOR_COMPONENT_G_BIT) |
                               static_cast<uint32_t>(VK_COLOR_COMPONENT_B_BIT) |
                               static_cast<uint32_t>(VK_COLOR_COMPONENT_A_BIT);
  }
  lua_pop(state, 1);

  return blendMode;
}

// Either { texture, texture, ... }
// Or, { { texture = t, layer = n, location = n, blendmode = {...}, clearvalue = {r,g,b,a} }, ... }
//
// blendmode: { "none"|"alpha"|"add"|"sub"|"mul", "alphamultiply"|"premultiplied" }
// Or, { srccolor = "", dstcolor = "", srcalpha = "", dstalpha = "", opcolor = "", opalpha = "" }
auto wrap_SetRenderTargets(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  std::vector<Ref<RenderTarget::RenderTarget>> renderTargets;

  luaL_checktype(state, 1, LUA_TTABLE);
  size_t len = lua_objlen(state, 1);

  bool isDetailed = false;
  lua_rawgeti(state, 1, 1);
  if (lua_istable(state, -1)) {
    lua_getfield(state, -1, "texture");
    if (!lua_isnil(state, -1)) {
      isDetailed = true;
    }
    lua_pop(state, 1);
  }
  lua_pop(state, 1);

  if (!isDetailed) {
    for (size_t i = 0; i < len; ++i) {
      auto *texture = LuaWrap::FromLuaObject<Texture::Texture>(
          state, static_cast<int>(i + 1));

      auto rendertarget = Ref<RenderTarget::RenderTarget>::Make();
      rendertarget->texture = *texture;
      renderTargets.push_back(rendertarget);

      lua_pop(state, 1);
    }
  } else {
    for (size_t i = 0; i < len; ++i) {
      lua_rawgeti(state, 1, static_cast<int>(i + 1));
      luaL_checktype(state, -1, LUA_TTABLE);

      auto rendertarget = Ref<RenderTarget::RenderTarget>::Make();

      // texture
      lua_getfield(state, -1, "texture");
      auto *texture = LuaWrap::FromLuaObject<Texture::Texture>(state, -1);
      rendertarget->texture = *texture;
      lua_pop(state, 1);

      // layer
      lua_getfield(state, -1, "layer");
      if (lua_isnumber(state, -1) != 0) {
        rendertarget->layer = static_cast<int>(lua_tointeger(state, -1));
      }
      lua_pop(state, 1);

      // location
      lua_getfield(state, -1, "location");
      if (lua_isnumber(state, -1) != 0) {
        rendertarget->location = static_cast<int>(lua_tointeger(state, -1));
      }
      lua_pop(state, 1);

      // blendmode
      lua_getfield(state, -1, "blendmode");
      if (lua_istable(state, -1)) {
        rendertarget->blendMode = FromLuaState(state);
      }
      lua_pop(state, 1);
    }
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