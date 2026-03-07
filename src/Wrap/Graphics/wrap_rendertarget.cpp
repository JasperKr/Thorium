#include "wrap_rendertarget.hpp"

#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Wrap/Graphics/wrap_color.hpp"
#include "Wrap/wrap.hpp"
#include "tl/expected.hpp"
#include <string>

#include "lua.hpp"
#include "vulkan/vulkan_core.h"
#include <cstring>
#include <vector>
namespace Graphics::DynamicRendering {
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

// blendmode: { blendmode = "none"|"alpha"|"add"|"sub"|"mul", alphamode = "alphamultiply"|"premultiplied", mask = "" }
// Or, { srccolor = "", dstcolor = "", srcalpha = "", dstalpha = "", opcolor = "", opalpha = "", mask = "" }
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto inline FromLuaState(lua_State *state)
    -> Result<VkPipelineColorBlendAttachmentState> {
  // blend mode from lua top of stack
  VkPipelineColorBlendAttachmentState blendMode = DefaultBlendMode;

  // assume stack is now [stuff, blendmode table]

  lua_getfield(state, -1, "blendmode");     // get blendmode field
  if (lua_type(state, -1) == LUA_TSTRING) { // Simple blendmode
    lua_getfield(state, -2, "alphamode");   // get alphamode field

    if (lua_isstring(state, -1) == 0) {
      return Error::Unexpected(
          "Expected string as second argument for blendmode");
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
      return Error::Unexpected("Invalid blend mode: " + std::string(modeStr));
    }

    const char *alphaModeStr = luaL_checkstring(state, -1);
    if (strcmp(alphaModeStr, "alphamultiply") == 0) {
      // No change needed
    } else if (strcmp(alphaModeStr, "premultiplied") == 0) {
      blendMode.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    } else {
      return Error::Unexpected("Invalid alpha blend mode: " +
                               std::string(alphaModeStr));
    }

    lua_pop(state, 2); // pop blendmode string and alphamode string
  } else {
    lua_pop(state, 1); // pop non-string blendmode field

    // Detailed blendmode
    lua_getfield(state, -1, "srccolor");
    const char *srcColorStr = luaL_checkstring(state, -1);
    lua_pop(state, 1);

    lua_getfield(state, -1, "dstcolor");
    const char *dstColorStr = luaL_checkstring(state, -1);
    lua_pop(state, 1);

    lua_getfield(state, -1, "srcalpha");
    const char *srcAlphaStr = luaL_checkstring(state, -1);
    lua_pop(state, 1);

    lua_getfield(state, -1, "dstalpha");
    const char *dstAlphaStr = luaL_checkstring(state, -1);
    lua_pop(state, 1);

    lua_getfield(state, -1, "colorop");
    const char *opColorStr = luaL_checkstring(state, -1);
    lua_pop(state, 1);

    lua_getfield(state, -1, "alphaop");
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
  if (lua_type(state, -1) == LUA_TSTRING) {
    constexpr uint32_t MaxMaskLength = 5; // 4 chars + null terminator
    const char *maskStr = luaL_checkstring(state, -1);

    if (strlen(maskStr) >= MaxMaskLength) {
      return Error::Unexpected("Blend mask string too long: " +
                               std::string(maskStr));
    }

    blendMode.colorWriteMask = 0;
    for (size_t i = 0; i < strlen(maskStr); i++) {
      auto character = maskStr[i]; // NOLINT
      switch (character) {
      case 'r':
        if ((blendMode.colorWriteMask & VK_COLOR_COMPONENT_R_BIT) != 0) {
          return Error::Unexpected("Duplicate 'r' in blend mask: " +
                                   std::string(maskStr));
        }
        blendMode.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
        break;
      case 'g':
        if ((blendMode.colorWriteMask & VK_COLOR_COMPONENT_G_BIT) != 0) {
          return Error::Unexpected("Duplicate 'g' in blend mask: " +
                                   std::string(maskStr));
        }
        blendMode.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
        break;
      case 'b':
        if ((blendMode.colorWriteMask & VK_COLOR_COMPONENT_B_BIT) != 0) {
          return Error::Unexpected("Duplicate 'b' in blend mask: " +
                                   std::string(maskStr));
        }
        blendMode.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
        break;
      case 'a':
        if ((blendMode.colorWriteMask & VK_COLOR_COMPONENT_A_BIT) != 0) {
          return Error::Unexpected("Duplicate 'a' in blend mask: " +
                                   std::string(maskStr));
        }
        blendMode.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
        break;
      default:
        return Error::Unexpected("Invalid character in blend mask: " +
                                 std::string(1, character));
      }
    }
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
    -> Ref<Graphics::DynamicRendering::RenderTarget> {
  luaL_checktype(state, index, LUA_TUSERDATA);

  auto *texture =
      LuaWrap::ObjectFromLua<Graphics::Texture::Texture>(state, index);

  if (texture == nullptr) {
    auto *ctx = Graphics::GetCurrentGraphicsContext();

    texture = ctx->swapchainInfo.textures[ctx->swapchainImageIndex].get();
  }

  auto rendertarget = Ref<Graphics::DynamicRendering::RenderTarget>::Make();
  rendertarget->texture = Ref<Graphics::Texture::Texture>(texture);
  rendertarget->blendMode = DefaultBlendMode;
  rendertarget->clearValue = {0.0F, 0.0F, 0.0F, 1.0F};
  rendertarget->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

  return rendertarget;
}

auto RenderTargetsFromOptions(lua_State *state, int index)
    -> Result<Ref<Graphics::DynamicRendering::RenderTarget>> {

  luaL_checktype(state, index, LUA_TTABLE);

  auto rendertarget = Ref<Graphics::DynamicRendering::RenderTarget>::Make();

  // check if element table[1] is a texture, if so, error,
  // as we expect it to be a named field

  lua_rawgeti(state, index, 1);
  if (lua_isnoneornil(state, -1) == 0) {
    if (LuaWrap::IsType<Graphics::Texture::Texture>(state, -1)) {
      return Error::Unexpected("Expected named field 'texture' in options "
                               "table, got texture at index 1.");
    }

    return Error::Unexpected(
        "Expected named fields in render target options table");
  }
  lua_pop(state, 1);

  // texture
  lua_getfield(state, index, "texture");
  if (lua_isnoneornil(state, -1) != 0) {
    auto *context = Graphics::GetCurrentGraphicsContext();

    rendertarget->texture =
        context->swapchainInfo.textures[context->swapchainImageIndex];
  } else {
    auto *texture =
        LuaWrap::ObjectFromLua<Graphics::Texture::Texture>(state, -1);
    if (texture == nullptr) {
      return Error::Unexpected("Invalid texture in render target options");
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
    auto blendModeResult = FromLuaState(state);
    if (Error::IsError(blendModeResult)) {
      return blendModeResult.error().AsUnexpected();
    }
    rendertarget->blendMode = blendModeResult.value();
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
      return Error::Unexpected(std::string("Invalid loadas mode: ") +
                               loadasStr);
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
//
// TODO: add support for settings parameter for last argument in varargs texture without settings mode
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto wrap_SetRenderTargets(lua_State *state) -> int {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  std::vector<Ref<Graphics::DynamicRendering::RenderTarget>> renderTargets;

  if (lua_gettop(state) == 0) {
    // No arguments, reset to default

    auto rendertarget = Ref<Graphics::DynamicRendering::RenderTarget>::Make();

    rendertarget->blendMode = DefaultBlendMode;
    rendertarget->clearValue = {0.0F, 0.0F, 0.0F, 1.0F};
    rendertarget->texture =
        ctx->swapchainInfo.textures[ctx->swapchainImageIndex];
    rendertarget->location = 0;
    rendertarget->layer = 0;
    rendertarget->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

    auto setResult =
        Graphics::DynamicRendering::SetRenderTargets({rendertarget});

    if (Error::IsError(setResult)) {
      return luaL_error(state, setResult.message.c_str());
    }

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

    auto isOptionsVarargs = lua_istable(state, 1) != 0;

    for (int i = 1; i <= nArgs; ++i) {
      // if table, treat as render target options
      if (lua_istable(state, i) != 0) {
        if (!isOptionsVarargs) {
          return luaL_error(
              state, "Expected Texture as argument %d to setRenderTargets", i);
        }

        auto renderTargetResult = RenderTargetsFromOptions(state, i);
        if (Error::IsError(renderTargetResult)) {
          return luaL_error(state, renderTargetResult.error().message.c_str());
        }
        renderTargets.emplace_back(renderTargetResult.value());
        continue;
      }

      if (isOptionsVarargs) {
        return luaL_error(
            state, "Expected table of render target options as argument %d", i);
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
        auto renderTargetResult = RenderTargetsFromOptions(state, -1);
        if (Error::IsError(renderTargetResult)) {
          return luaL_error(state, "Error parsing render target options: %s",
                            renderTargetResult.error().message.c_str());
        }
        renderTargets.emplace_back(renderTargetResult.value());
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

  auto setResult = Graphics::DynamicRendering::SetRenderTargets(renderTargets);
  if (Error::IsError(setResult)) {
    return luaL_error(state, setResult.message.c_str());
  }
  return 0;
}
} // namespace Graphics::DynamicRendering