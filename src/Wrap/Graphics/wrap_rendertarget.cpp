#include "wrap_rendertarget.hpp"

#include "Graphics/graphics.hpp"
#include "Graphics/rendertarget.hpp"
#include "Graphics/texture.hpp"
#include "Modules/console.hpp"
#include "Wrap/Graphics/wrap_color.hpp"
#include "Wrap/wrap.hpp"
#include "vulkan/vulkan_core.h"
#include <cstring>
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#include <vector>
namespace Graphics::RenderTarget {
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
  return VK_BLEND_FACTOR_ONE; // Default
}

// blendmode: { blendmode = "none"|"alpha"|"add"|"sub"|"mul", alphamode = "alphamultiply"|"premultiplied", mask = int }
// Or, { srccolor = "", dstcolor = "", srcalpha = "", dstalpha = "", opcolor = "", opalpha = "", mask = int }
auto inline FromLuaState(lua_State *state)
    -> VkPipelineColorBlendAttachmentState {
  // blend mode from lua top of stack
  VkPipelineColorBlendAttachmentState blendMode = {};

  // assume stack is now [stuff, blendmode table]

  lua_getfield(state, -1, "blendmode");   // get blendmode field
  if (lua_isstring(state, -1) != 0) {     // Simple blendmode
    lua_getfield(state, -2, "alphamode"); // get alphamode field

    if (lua_isstring(state, -1) == 0) {
      luaL_error(state, "Expected string as second argument for blendmode");
    }
    // [stuff, blendmode table, blendmode string, alphamode string]

    const char *modeStr = luaL_checkstring(state, -2);
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

    const char *alphaModeStr = luaL_checkstring(state, -1);
    if (strcmp(alphaModeStr, "alphamultiply") == 0) {
      // No change needed
    } else if (strcmp(alphaModeStr, "premultiplied") == 0) {
      blendMode.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    } else {
      luaL_error(state, "Invalid alpha blend mode: %s", alphaModeStr);
    }

    blendMode.colorWriteMask = static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) |
                               static_cast<uint32_t>(VK_COLOR_COMPONENT_G_BIT) |
                               static_cast<uint32_t>(VK_COLOR_COMPONENT_B_BIT) |
                               static_cast<uint32_t>(VK_COLOR_COMPONENT_A_BIT);

    lua_pop(state, 2); // pop blendmode string and alphamode string
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
  lua_getfield(state, -1, "mask");
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

auto RenderTargetsFromTexture(lua_State *state, int index)
    -> Ref<Graphics::RenderTarget::RenderTarget> {
  luaL_checktype(state, index, LUA_TUSERDATA);

  auto *texture =
      LuaWrap::FromLuaObject<Graphics::Texture::Texture>(state, index);

  if (texture == nullptr) {
    auto *ctx = Graphics::GetCurrentGraphicsContext();
    texture = GetSwapchainTextures()[ctx->swapchainImageIndex].get();
  }

  auto rendertarget = Ref<Graphics::RenderTarget::RenderTarget>::Make();
  rendertarget->texture = Ref<Graphics::Texture::Texture>(texture);
  rendertarget->blendMode = DefaultBlendMode;
  rendertarget->clearValue = {0.0F, 0.0F, 0.0F, 1.0F};

  return rendertarget;
}

auto RenderTargetsFromOptions(lua_State *state, int index)
    -> Ref<Graphics::RenderTarget::RenderTarget> {

  luaL_checktype(state, index, LUA_TTABLE);

  auto rendertarget = Ref<Graphics::RenderTarget::RenderTarget>::Make();

  // texture
  lua_getfield(state, index, "texture");
  if (lua_isnoneornil(state, -1) != 0) {
    auto *context = Graphics::GetCurrentGraphicsContext();
    rendertarget->texture =
        GetSwapchainTextures()[context->swapchainImageIndex];
  } else {
    auto *texture =
        LuaWrap::FromLuaObject<Graphics::Texture::Texture>(state, -1);
    if (texture == nullptr) {
      PrintError("Expected Texture as rendertarget texture");
      return {};
    }
    rendertarget->texture = Ref<Graphics::Texture::Texture>(texture);
  }
  lua_pop(state, 1);

  // layer
  lua_getfield(state, index, "layer");
  if (lua_isnumber(state, -1) != 0) {
    rendertarget->layer = static_cast<int>(lua_tointeger(state, -1));
  }
  lua_pop(state, 1);

  // location
  lua_getfield(state, index, "location");
  if (lua_isnumber(state, -1) != 0) {
    rendertarget->location = static_cast<int>(lua_tointeger(state, -1));
  }
  lua_pop(state, 1);

  // blendmode
  lua_getfield(state, index, "blendmode");
  if (lua_istable(state, -1) != 0) {
    rendertarget->blendMode = FromLuaState(state);
  } else {
    rendertarget->blendMode = DefaultBlendMode;
  }
  lua_pop(state, 1);

  // loadas:
  // { r, g, b, a } | "clear" (0,0,0,1) | "load" (default) | "none" (don't care)
  lua_getfield(state, index, "loadas");
  if (lua_istable(state, -1) != 0) {
    auto color = ColorFromLuaState(state, ColorFormat::List, -1);

    rendertarget->clearValue.color.float32[0] = color.r;
    rendertarget->clearValue.color.float32[1] = color.g;
    rendertarget->clearValue.color.float32[2] = color.b;
    rendertarget->clearValue.color.float32[3] = color.a;

    rendertarget->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  } else if (lua_isstring(state, -1) != 0) {
    const char *loadasStr = luaL_checkstring(state, -1);
    if (strcmp(loadasStr, "clear") == 0) {
      rendertarget->clearValue.color.float32[0] = 0.0F;
      rendertarget->clearValue.color.float32[1] = 0.0F;
      rendertarget->clearValue.color.float32[2] = 0.0F;
      rendertarget->clearValue.color.float32[3] = 1.0F;
      rendertarget->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    } else if (strcmp(loadasStr, "load") == 0) {
      rendertarget->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    } else if (strcmp(loadasStr, "none") == 0) {
      rendertarget->loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    } else {
      luaL_error(state, "Invalid loadas value: %s", loadasStr);
    }
  }

  lua_pop(state, 2);

  return rendertarget;
}

auto IsOptionsTable(lua_State *state, int index) -> bool {
  luaL_checktype(state, index, LUA_TTABLE);

  lua_getfield(state, index, "texture");
  if (lua_isnoneornil(state, -1) == 0) {
    lua_pop(state, 1);
    return true;
  }
  lua_pop(state, 1);

  lua_getfield(state, index, "loadas");
  if (lua_isnoneornil(state, -1) == 0) {
    lua_pop(state, 1);
    return true;
  }
  lua_pop(state, 1);

  lua_getfield(state, index, "blendmode");
  if (lua_isnoneornil(state, -1) == 0) {
    lua_pop(state, 1);
    return true;
  }
  lua_pop(state, 1);

  lua_getfield(state, index, "layer");
  if (lua_isnoneornil(state, -1) == 0) {
    lua_pop(state, 1);
    return true;
  }
  lua_pop(state, 1);

  lua_getfield(state, index, "location");
  if (lua_isnoneornil(state, -1) == 0) {
    lua_pop(state, 1);
    return true;
  }
  lua_pop(state, 1);

  return false;
}

// Variants:
// Varargs
// (texture, texture, ... )
// ( { texture = t, ... }, { ... })
// Table
// { texture, texture, ... }
// { { texture = t, layer = n, location = n, blendmode = {...}, loadas = {r,g,b,a} }, ... }
//
// blendmode: { "none"|"alpha"|"add"|"sub"|"mul", "alphamultiply"|"premultiplied" }
// Or, { srccolor = "", dstcolor = "", srcalpha = "", dstalpha = "", opcolor = "", opalpha = "" }
auto wrap_SetRenderTargets(lua_State *state) -> int {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  std::vector<Ref<Graphics::RenderTarget::RenderTarget>> renderTargets;

  if (lua_gettop(state) == 0) {
    // No arguments, reset to default
    Graphics::RenderTarget::SetRenderTargets({});
    return 0;
  }

  bool hasVarargs = false;

  if (lua_isuserdata(state, 1) != 0) {
    hasVarargs = true;
  } else if (lua_istable(state, 1) != 0) {
    hasVarargs = IsOptionsTable(state, 1);
  }

  if (hasVarargs) {
    // vararg textures or vararg render target options

    // loop over arguments and get info
    int nArgs = lua_gettop(state);

    for (int i = 1; i <= nArgs; ++i) {
      // if table, treat as render target options
      if (lua_istable(state, i) != 0) {
        auto renderTarget = RenderTargetsFromOptions(state, i);
        renderTargets.emplace_back(renderTarget);
        continue;
      }

      // else, treat as texture
      auto renderTarget = RenderTargetsFromTexture(state, i);
      renderTargets.emplace_back(renderTarget);
    }
  } else if (lua_istable(state, 1) != 0) {
    // table of textures or table of render target options

    // get length of table
    auto tableLength = static_cast<size_t>(lua_objlen(state, 1));

    for (size_t i = 0; i < tableLength; ++i) {
      lua_rawgeti(state, 1, static_cast<int>(i + 1));

      // if table, treat as render target options
      if (lua_istable(state, -1) != 0) {
        auto renderTarget = RenderTargetsFromOptions(state, -1);
        renderTargets.emplace_back(renderTarget);
        lua_pop(state, 1);
        continue;
      }

      // else, treat as texture
      auto renderTarget = RenderTargetsFromTexture(state, -1);
      renderTargets.emplace_back(renderTarget);
      lua_pop(state, 1);
    }
  } else {
    return luaL_error(state,
                      "Invalid arguments to RenderTarget.setRenderTargets");
  }

  Graphics::RenderTarget::SetRenderTargets(renderTargets);
  return 0;
}
} // namespace Graphics::RenderTarget