#include "Wrap/Graphics/wrap_graphics.hpp"

#include "Graphics/draw.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/render.hpp"
#include "Graphics/renderThread.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/vertexformat.hpp"
#include "Modules/color.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Wrap/Graphics/wrap_color.hpp"
#include "Wrap/wrap.hpp"
#include "tl/expected.hpp"
#include <cassert>

#include "lua.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <cstring>
#include <vector>

namespace Graphics {
auto wrap_Present(lua_State *state) -> int {
  auto &ctx = *GetCurrentGraphicsContext();
  auto result = Present(ctx);

  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.ToString().c_str());
  }

  return 0;
}

// RenderTarget functions
auto wrap_Push(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto pushError = DynamicRendering::Push(*ctx);
  if (Error::IsError(pushError)) {
    return luaL_error(state, "%s", pushError.ToString().c_str());
  }
  return 0;
}
auto wrap_Pop(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto error = DynamicRendering::Pop(*ctx);
  if (Error::IsError(error)) {
    return luaL_error(state, "%s", error.ToString().c_str());
  }

  return 0;
}
auto wrap_Reset(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto resetError = DynamicRendering::Reset(*ctx);
  if (Error::IsError(resetError)) {
    return luaL_error(state, "%s", resetError.ToString().c_str());
  }
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

  DynamicRendering::SetDepthMode(enableRead, writeEnable, compareOp);
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

  DynamicRendering::SetCullMode(cullMode);
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

  DynamicRendering::SetPolygonMode(polygonMode);
  return 0;
}

auto wrap_SetViewport(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  if (lua_gettop(state) == 0) {
    DynamicRendering::SetViewport(nullptr);
    return 0;
  }
  VkViewport viewport{};
  viewport.x = static_cast<float>(luaL_checknumber(state, 1));
  viewport.y = static_cast<float>(luaL_checknumber(state, 2));
  viewport.width = static_cast<float>(luaL_checknumber(state, 3));
  viewport.height = static_cast<float>(luaL_checknumber(state, 4));
  viewport.minDepth =
      static_cast<float>(luaL_optnumber(state, 5, 0.0)); // NOLINT
  viewport.maxDepth =
      static_cast<float>(luaL_optnumber(state, 6, 1.0)); // NOLINT
  DynamicRendering::SetViewport(&viewport);
  return 0;
}

auto wrap_SetScissor(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();

  if (lua_gettop(state) == 0) {
    DynamicRendering::SetScissor(nullptr);
    return 0;
  }

  VkRect2D scissor{};
  scissor.offset.x = static_cast<int32_t>(luaL_checkinteger(state, 1));
  scissor.offset.y = static_cast<int32_t>(luaL_checkinteger(state, 2));
  scissor.extent.width = static_cast<uint32_t>(luaL_checkinteger(state, 3));
  scissor.extent.height = static_cast<uint32_t>(luaL_checkinteger(state, 4));
  DynamicRendering::SetScissor(&scissor);
  return 0;
}

auto wrap_ClipScissor(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  VkRect2D scissor{};
  scissor.offset.x = static_cast<int32_t>(luaL_checkinteger(state, 1));
  scissor.offset.y = static_cast<int32_t>(luaL_checkinteger(state, 2));
  scissor.extent.width = static_cast<uint32_t>(luaL_checkinteger(state, 3));
  scissor.extent.height = static_cast<uint32_t>(luaL_checkinteger(state, 4));
  DynamicRendering::ClipScissor(scissor);
  return 0;
}

auto wrap_SetShader(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto *shaderHandle =
      LuaWrap::ObjectFromLua<Graphics::Shader::ShaderModule>(state, 1);
  DynamicRendering::SetShader(Ref<Shader::ShaderModule>(shaderHandle));
  return 0;
}

auto wrap_SetLineWidth(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto lineWidth = static_cast<float>(luaL_checknumber(state, 1));
  if (lineWidth <= 0.0F) {
    return luaL_error(state, "Line width must be greater than 0.");
  }
  DynamicRendering::SetLineWidth(lineWidth);
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

  DynamicRendering::SetWindingOrder(frontFace);
  return 0;
}

// Getters

// Options: "never", "less", "equal", "lequal", "greater", "notequal", "gequal", "always", writeEnable: bool
auto wrap_GetDepthMode(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  bool enableRead = false;
  bool writeEnable = false;
  VkCompareOp compareOp = VK_COMPARE_OP_MAX_ENUM;
  std::tie(enableRead, writeEnable, compareOp) =
      DynamicRendering::GetDepthMode();

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
  auto cullMode = DynamicRendering::GetCullMode();

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
  auto polygonMode = DynamicRendering::GetPolygonMode();

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
  auto viewport = DynamicRendering::GetViewport();

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
  auto scissor = DynamicRendering::GetScissor();

  lua_pushinteger(state, static_cast<lua_Integer>(scissor.offset.x));
  lua_pushinteger(state, static_cast<lua_Integer>(scissor.offset.y));
  lua_pushinteger(state, static_cast<lua_Integer>(scissor.extent.width));
  lua_pushinteger(state, static_cast<lua_Integer>(scissor.extent.height));
  return 4;
}

// Returns shader handle as integer
auto wrap_GetShader(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto ref = DynamicRendering::GetShader();

  const auto *type = Shader::ShaderModule::GetType();

  LuaWrap::PushObject(state, type, ref.get());

  return 1;
}

auto wrap_GetRenderTargets(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto renderTargets = DynamicRendering::GetRenderTargets();

  lua_newtable(state);
  for (size_t i = 0; i < renderTargets.size(); ++i) {
    auto &renderTarget = renderTargets[i];
    const auto *type = DynamicRendering::RenderTarget::GetType();

    LuaWrap::PushObject(state, type, renderTarget.get());
    lua_rawseti(state, -2, static_cast<int>(i + 1));
  }

  return 1;
}
auto wrap_GetLineWidth(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto lineWidth = DynamicRendering::GetLineWidth();
  lua_pushnumber(state, static_cast<lua_Number>(lineWidth));
  return 1;
}
auto wrap_GetWindingOrder(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto frontFace = DynamicRendering::GetWindingOrder();

  const char *order = "unknown";
  if (frontFace == VK_FRONT_FACE_CLOCKWISE) {
    order = "cw";
  } else if (frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE) {
    order = "ccw";
  }

  lua_pushstring(state, order);
  return 1;
}

struct FormatDefault2D {
  float position[2]; // NOLINT
  float texCoord[2]; // NOLINT
  uint32_t color;
};

// NOLINTNEXTLINE; Cache quad mesh to avoid recreating it every frame
thread_local Ref<Mesh> QuadMeshCache;

auto ShutdownWrapGraphics() -> void { QuadMeshCache.reset(); };

inline auto GetQuadMesh(GraphicsContext &context, const VkRect2D size,
                        Color color) -> Result<Ref<Mesh>> {
  ZoneScoped;
  // Create a quad mesh covering the given size NOLINTNEXTLINE
  thread_local std::vector<FormatDefault2D> vertices{4};
  thread_local std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

  vertices[0].position[0] = static_cast<float>(size.offset.x);
  vertices[0].position[1] = static_cast<float>(size.offset.y);
  vertices[0].texCoord[0] = 0.0F;
  vertices[0].texCoord[1] = 0.0F;
  vertices[0].color = color.Pack();

  vertices[1].position[0] =
      static_cast<float>(size.offset.x + size.extent.width);
  vertices[1].position[1] = static_cast<float>(size.offset.y);
  vertices[1].texCoord[0] = 1.0F;
  vertices[1].texCoord[1] = 0.0F;
  vertices[1].color = color.Pack();

  vertices[2].position[0] =
      static_cast<float>(size.offset.x + size.extent.width);
  vertices[2].position[1] =
      static_cast<float>(size.offset.y + size.extent.height);
  vertices[2].texCoord[0] = 1.0F;
  vertices[2].texCoord[1] = 1.0F;
  vertices[2].color = color.Pack();

  vertices[3].position[0] = static_cast<float>(size.offset.x);
  vertices[3].position[1] =
      static_cast<float>(size.offset.y + size.extent.height);
  vertices[3].texCoord[0] = 0.0F;
  vertices[3].texCoord[1] = 1.0F;
  vertices[3].color = color.Pack();

  static VertexFormat vertexFormat({
      VertexComponent{
          .name = "Position",
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
      },
      VertexComponent{
          .name = "TexCoord",
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
      },
      VertexComponent{
          .name = "Color",
          .location = 2,
          .binding = 0,
          .format = VK_FORMAT_R8G8B8A8_UNORM,
      },
  });

  // NOLINTNEXTLINE; Reinterpret cast is necessary here
  auto span = std::span<uint8_t>(reinterpret_cast<uint8_t *>(vertices.data()),
                                 vertexFormat.GetBindings()[0].stride *
                                     vertices.size());

  assert(sizeof(FormatDefault2D) == vertexFormat.GetBindings()[0].stride);

  if (QuadMeshCache.get() == nullptr) {
    auto meshResult = Mesh::Create(context, vertexFormat, span);

    if (Error::IsError(meshResult)) {
      return meshResult.error().AsUnexpected();
    }

    QuadMeshCache = meshResult.value();
  }

  auto mesh = QuadMeshCache;

  auto setDataError = mesh->SetVertices(context, span);
  if (Error::IsError(setDataError)) {
    return setDataError.AsUnexpected();
  }

  auto indexSpan = std::span<uint8_t>( // NOLINTNEXTLINE
      reinterpret_cast<uint8_t *>(indices.data()),
      indices.size() * GetIndexFormatSize(VK_INDEX_TYPE_UINT32));

  setDataError = mesh->SetIndices(context, indexSpan, VK_INDEX_TYPE_UINT32);
  if (Error::IsError(setDataError)) {
    return setDataError.AsUnexpected();
  }

  return mesh;
}

// texture | mesh
auto wrap_Draw(lua_State *state) -> int {
  ZoneScoped;
  auto *ctx = GetCurrentGraphicsContext();

  Ref<Mesh> mesh;

  if (LuaWrap::IsType<Texture::Texture>(state, 1)) {
    auto *texture = LuaWrap::ObjectFromLua<Texture::Texture>(state, 1);
    auto result =
        GetQuadMesh(*ctx,
                    VkRect2D{
                        .offset = {0, 0},
                        .extent = {texture->GetWidth(), texture->GetHeight()},
                    },
                    Color(1.0F, 1.0F, 1.0F, 1.0F));
    if (Error::IsError(result)) {
      return luaL_error(state, "%s", result.error().ToString().c_str());
    }

    if (texture != nullptr) {
      auto shader = DynamicRendering::GetShader();
      if (shader.get() == nullptr) {
        shader = Shader::DefaultShaderModule;
      }

      auto sendResult = shader->Send(*ctx, {"MainTexture"}, texture);
      if (Error::IsError(sendResult)) {
        return luaL_error(state, "%s", sendResult.ToString().c_str());
      }
    } else {
      return luaL_error(state, "Texture is null.");
    }

    mesh = result.value();
  } else if (LuaWrap::IsType<Mesh>(state, 1)) {
    mesh = Ref<Mesh>(LuaWrap::ObjectFromLua<Mesh>(state, 1));
  } else {
    return luaL_error(state, "Invalid argument to draw.");
  }

  if (mesh.get() == nullptr) {
    return luaL_error(state, "Mesh is null.");
  }

  auto instanceCount = 1U;

  if (lua_gettop(state) >= 2) {
    instanceCount = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  }

  auto drawResult = Draw(*ctx, *mesh, instanceCount);

  if (Error::IsError(drawResult)) {
    return luaL_error(state, "%s", drawResult.ToString().c_str());
  }

  return 0;
}

auto wrap_Dispatch(lua_State *state) -> int {
  ZoneScoped;
  auto *ctx = GetCurrentGraphicsContext();

  Math::Uvec3 threadgroups{1, 1, 1};
  threadgroups.x = static_cast<uint32_t>(luaL_checkinteger(state, 1));
  threadgroups.y = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  threadgroups.z = static_cast<uint32_t>(luaL_checkinteger(state, 3));

  auto dispatchResult = Dispatch(*ctx, threadgroups);

  if (Error::IsError(dispatchResult)) {
    return luaL_error(state, "%s", dispatchResult.ToString().c_str());
  }

  return 0;
}

auto wrap_DispatchIndirect(lua_State *state) -> int {
  ZoneScoped;
  auto *ctx = GetCurrentGraphicsContext();

  auto *bufferHandle = LuaWrap::ObjectFromLua<Graphics::Buffer>(state, 1);
  Ref<Buffer> indirectBuffer(bufferHandle);

  auto offset = static_cast<VkDeviceSize>(luaL_optinteger(state, 2, 0));
  auto dispatchResult = DispatchIndirect(*ctx, indirectBuffer, offset);

  if (Error::IsError(dispatchResult)) {
    return luaL_error(state, "%s", dispatchResult.ToString().c_str());
  }

  return 0;
}

auto wrap_DrawIndirect(lua_State *state) -> int {
  ZoneScoped;
  auto *ctx = GetCurrentGraphicsContext();

  Ref<Mesh> mesh;

  if (LuaWrap::IsType<Mesh>(state, 1)) {
    mesh = Ref<Mesh>(LuaWrap::ObjectFromLua<Mesh>(state, 1));
  } else {
    return luaL_error(state, "Invalid argument to drawIndirect.");
  }

  if (mesh.get() == nullptr) {
    return luaL_error(state, "Mesh is null.");
  }

  auto *bufferHandle = LuaWrap::ObjectFromLua<Graphics::Buffer>(state, 2);
  Ref<Buffer> indirectBuffer(bufferHandle);

  auto offset = static_cast<VkDeviceSize>(luaL_optinteger(state, 3, 0));
  auto count = static_cast<uint32_t>(luaL_optinteger(state, 4, 1));
  auto drawResult = DrawIndirect(*ctx, *mesh, indirectBuffer, offset, count);

  if (Error::IsError(drawResult)) {
    return luaL_error(state, "%s", drawResult.ToString().c_str());
  }

  return 0;
}

// Clear the current render target with the given color
// Either: bool (0,0,0,1), bool (depth), bool (stencil)
// Or: Color (attachment 1, vararg), value (depth), value (stencil)
// Or: {[1..]: Color (attachment), depth=value, stencil=value}
auto wrap_Clear(lua_State *state) -> int {
  ZoneScoped;
  auto *ctx = GetCurrentGraphicsContext();

  DynamicRendering::ClearInfo clearInfo{};

  if (lua_isboolean(state, 1) != 0) {
    // bool version
    bool clear = lua_toboolean(state, 1) != 0;
    if (clear) {
      clearInfo.colors.emplace_back(0.0F, 0.0F, 0.0F, 1.0F);
    }

    clearInfo.clearDepth =
        lua_isboolean(state, 2) != 0 ? (lua_toboolean(state, 2) != 0) : false;
    clearInfo.clearStencil =
        lua_isboolean(state, 3) != 0 ? (lua_toboolean(state, 3) != 0) : false;
  } else if (lua_isnumber(state, 1) != 0) {
    // Color, depth, stencil version
    auto color = ColorFromLuaState(state, ColorFormat::VarArg, 1);
    clearInfo.colors.emplace_back(color);

    if (lua_isnumber(state, 5) != 0) { // NOLINT
      clearInfo.depthClearValue =
          static_cast<float>(lua_tonumber(state, 5)); // NOLINT
    }

    if (lua_isnumber(state, 6) != 0) { // NOLINT
      clearInfo.stencilClearValue =
          static_cast<int>(lua_tointeger(state, 6)); // NOLINT
    }
  } else if (lua_istable(state, 1) != 0) {
    // Table version
    lua_pushnil(state);
    while (lua_next(state, 1) != 0) {
      // key at -2, value at -1
      if (lua_type(state, -2) == LUA_TNUMBER) {
        // Color entry
        auto color = ColorFromLuaState(state, ColorFormat::List, -1);
        clearInfo.colors.emplace_back(color);
      } else if (lua_type(state, -2) == LUA_TSTRING) {
        const char *key = lua_tostring(state, -2);
        if (strcmp(key, "depth") == 0) {
          clearInfo.depthClearValue =
              static_cast<float>(lua_tonumber(state, -1)); // NOLINT
        } else if (strcmp(key, "stencil") == 0) {
          clearInfo.stencilClearValue =
              static_cast<int>(lua_tointeger(state, -1)); // NOLINT
        }
      }
      lua_pop(state, 1); // pop value, keep key for next iteration
    }
  }

  auto result = DynamicRendering::Clear(*ctx, clearInfo);

  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.ToString().c_str());
  }

  return 0;
}

/*
auto wrap_GetWidth(lua_State *state) -> int;
auto wrap_GetHeight(lua_State *state) -> int;
auto wrap_GetDimensions(lua_State *state) -> int;
*/

auto wrap_GetWidth(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto width = ctx->swapchainInfo.extent.width;
  lua_pushinteger(state, static_cast<lua_Integer>(width));
  return 1;
}

auto wrap_GetHeight(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto height = ctx->swapchainInfo.extent.height;
  lua_pushinteger(state, static_cast<lua_Integer>(height));
  return 1;
}

auto wrap_GetDimensions(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();
  auto extent = ctx->swapchainInfo.extent;

  lua_pushinteger(state, static_cast<lua_Integer>(extent.width));
  lua_pushinteger(state, static_cast<lua_Integer>(extent.height));
  return 2;
}

auto wrap_AquireCommandBuffer(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current graphics context.");
  }

  Threading::AquireInfo info{};
  info.name = luaL_checkstring(state, 1);
  info.priority = static_cast<int>(luaL_optinteger(state, 2, 0));

  auto result = Threading::AquireCommandBuffer(*ctx, info);
  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.error().message.c_str());
  }

  return 0;
}

auto wrap_SubmitCommandBuffer(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current graphics context.");
  }

  auto submitResult = Threading::SubmitCommands(*ctx);
  if (Error::IsError(submitResult)) {
    return luaL_error(state, "%s", submitResult.ToString().c_str());
  }

  return 0;
}

auto wrap_UseCommands(lua_State *state) -> int {

  if (lua_type(state, 1) == LUA_TNUMBER) {
    Graphics::UseCommands(static_cast<uint64_t>(luaL_checkinteger(state, 1)));
  } else if (lua_type(state, 1) == LUA_TSTRING) {
    Graphics::UseCommands(luaL_checkstring(state, 1));
  } else {
    return luaL_error(state, "Invalid argument to useCommands.");
  }

  return 0;
}

// Get a list of command buffer names
// Optionally a table to fill
auto wrap_GetGeneratedCommands(lua_State *state) -> int {
  if (lua_gettop(state) == 1 && lua_istable(state, 1) == 0) {
    return luaL_error(state, "Expected table as argument.");
  }

  if (lua_gettop(state) == 0) {
    lua_newtable(state); // Create new table
  }

  auto commands = Graphics::Threading::GetGeneratedCommands();

  for (size_t i = 0; i < commands.size(); ++i) {
    lua_pushstring(state, commands[i].c_str());
    lua_rawseti(state, -2, static_cast<int>(i + 1));
  }

  return 1;
}

} // namespace Graphics