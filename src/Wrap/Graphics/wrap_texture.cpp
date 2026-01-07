#include "Graphics/format.hpp"
#include "Graphics/texture.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/image.hpp"
#include "Wrap/wrap.hpp"
#include <set>
#include <string>

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
namespace Graphics::Texture {
auto inline StringToVkFilter(const char *filterStr) -> VkFilter {
  if (strcmp(filterStr, "nearest") == 0) {
    return VK_FILTER_NEAREST;
  }
  if (strcmp(filterStr, "linear") == 0) {
    return VK_FILTER_LINEAR;
  }
  return VK_FILTER_LINEAR; // Default
}

auto wrap_SetFilter(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  const char *minFilterStr = luaL_checkstring(state, 2);
  const char *magFilterStr = luaL_checkstring(state, 3);
  const char *mipFilterStr = luaL_checkstring(state, 4);

  VkFilter minFilter = StringToVkFilter(minFilterStr);
  VkFilter magFilter = StringToVkFilter(magFilterStr);
  VkFilter mipFilter_ = StringToVkFilter(mipFilterStr);
  auto mipFilter = (mipFilter_ == VK_FILTER_NEAREST)
                       ? VK_SAMPLER_MIPMAP_MODE_NEAREST
                       : VK_SAMPLER_MIPMAP_MODE_LINEAR;

  texture->SetFilter(minFilter, magFilter, mipFilter);

  return 0;
}

auto wrap_GetFilter(lua_State *state) -> int {
  PrintWarning("Getting texture filter");
  PrintWarning("Texture type ptr: {}", (void *)Texture::GetType());
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  VkFilter minFilter = VK_FILTER_LINEAR;
  VkFilter magFilter = VK_FILTER_LINEAR;
  VkSamplerMipmapMode mipFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  std::tie(minFilter, magFilter, mipFilter) = texture->GetFilter();

  const char *minFilterStr =
      (minFilter == VK_FILTER_NEAREST) ? "nearest" : "linear";
  const char *magFilterStr =
      (magFilter == VK_FILTER_NEAREST) ? "nearest" : "linear";
  const char *mipFilterStr =
      (mipFilter == VK_SAMPLER_MIPMAP_MODE_NEAREST) ? "nearest" : "linear";

  lua_pushstring(state, minFilterStr);
  lua_pushstring(state, magFilterStr);
  lua_pushstring(state, mipFilterStr);

  return 3;
}

auto wrap_SetAnisotropy(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  auto anisotropy = static_cast<float>(luaL_checknumber(state, 2));

  texture->SetAnisotropy(anisotropy);

  return 0;
}

auto wrap_GetAnisotropy(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  float anisotropy = texture->GetAnisotropy();

  lua_pushnumber(state, static_cast<lua_Number>(anisotropy));
  return 1;
}

auto inline StringToAddressMode(const char *addressModeStr)
    -> VkSamplerAddressMode {
  if (strcmp(addressModeStr, "repeat") == 0) {
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
  if (strcmp(addressModeStr, "mirrored_repeat") == 0) {
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  }
  if (strcmp(addressModeStr, "clamp_to_edge") == 0) {
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
  return VK_SAMPLER_ADDRESS_MODE_REPEAT; // Default
}

auto wrap_SetWrapmode(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  const char *addressModeUStr = luaL_checkstring(state, 2);
  const char *addressModeVStr = luaL_checkstring(state, 3);
  const char *addressModeWStr = luaL_checkstring(state, 4);

  VkSamplerAddressMode addressModeU = StringToAddressMode(addressModeUStr);
  VkSamplerAddressMode addressModeV = StringToAddressMode(addressModeVStr);
  VkSamplerAddressMode addressModeW = StringToAddressMode(addressModeWStr);

  texture->SetWrapmode(addressModeU, addressModeV, addressModeW);

  return 0;
}

auto inline AddressModeToString(VkSamplerAddressMode addressMode) -> const
    char * {
  switch (addressMode) {
  case VK_SAMPLER_ADDRESS_MODE_REPEAT:
    return "repeat";
  case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
    return "mirrored_repeat";
  case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
    return "clamp_to_edge";
  default:
    return "repeat";
  }
}

auto wrap_GetWrapmode(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  std::tie(addressModeU, addressModeV, addressModeW) = texture->GetWrap();

  const char *addressModeUStr = AddressModeToString(addressModeU);
  const char *addressModeVStr = AddressModeToString(addressModeV);
  const char *addressModeWStr = AddressModeToString(addressModeW);

  lua_pushstring(state, addressModeUStr);
  lua_pushstring(state, addressModeVStr);
  lua_pushstring(state, addressModeWStr);

  return 3;
}

auto wrap_SetLodBias(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  auto mipLodBias = static_cast<float>(luaL_checknumber(state, 2));

  texture->SetLodBias(mipLodBias);

  return 0;
}

auto wrap_GetLodBias(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  float mipLodBias = texture->GetLodBias();

  lua_pushnumber(state, static_cast<lua_Number>(mipLodBias));
  return 1;
}

auto wrap_SetLodRange(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  auto minLod = static_cast<float>(luaL_checknumber(state, 2));
  auto maxLod = static_cast<float>(luaL_checknumber(state, 3));

  texture->SetLodRange(minLod, maxLod);

  return 0;
}

auto wrap_GetLodRange(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  float minLod = 0.0F;
  float maxLod = 0.0F;
  std::tie(minLod, maxLod) = texture->GetLodRange();

  lua_pushnumber(state, static_cast<lua_Number>(minLod));
  lua_pushnumber(state, static_cast<lua_Number>(maxLod));
  return 2;
}

inline auto StringToCompareOp(const char *compareOpStr) -> VkCompareOp {
  if (strcmp(compareOpStr, "never") == 0) {
    return VK_COMPARE_OP_NEVER;
  }
  if (strcmp(compareOpStr, "less") == 0) {
    return VK_COMPARE_OP_LESS;
  }
  if (strcmp(compareOpStr, "equal") == 0) {
    return VK_COMPARE_OP_EQUAL;
  }
  if (strcmp(compareOpStr, "lequal") == 0) {
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  }
  if (strcmp(compareOpStr, "greater") == 0) {
    return VK_COMPARE_OP_GREATER;
  }
  if (strcmp(compareOpStr, "notequal") == 0) {
    return VK_COMPARE_OP_NOT_EQUAL;
  }
  if (strcmp(compareOpStr, "gequal") == 0) {
    return VK_COMPARE_OP_GREATER_OR_EQUAL;
  }
  if (strcmp(compareOpStr, "always") == 0) {
    return VK_COMPARE_OP_ALWAYS;
  }
  return VK_COMPARE_OP_ALWAYS; // Default
}

inline auto CompareOpToString(VkCompareOp compareOp) -> const char * {
  switch (compareOp) {
  case VK_COMPARE_OP_NEVER:
    return "never";
  case VK_COMPARE_OP_LESS:
    return "less";
  case VK_COMPARE_OP_EQUAL:
    return "equal";
  case VK_COMPARE_OP_LESS_OR_EQUAL:
    return "lequal";
  case VK_COMPARE_OP_GREATER:
    return "greater";
  case VK_COMPARE_OP_NOT_EQUAL:
    return "notequal";
  case VK_COMPARE_OP_GREATER_OR_EQUAL:
    return "gequal";
  case VK_COMPARE_OP_ALWAYS:
  default:
    return "always";
  }
}

auto wrap_SetDepthCompare(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  if (lua_isnoneornil(state, 2) != 0) {
    texture->SetDepthCompare(false, VK_COMPARE_OP_ALWAYS);
    return 0;
  }

  auto compareOp = StringToCompareOp(luaL_checkstring(state, 2));
  auto enable = compareOp != VK_COMPARE_OP_ALWAYS;
  texture->SetDepthCompare(enable, compareOp);

  return 0;
}

auto wrap_GetDepthCompare(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  bool enable = false;
  VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
  std::tie(enable, compareOp) = texture->GetDepthCompare();

  lua_pushstring(state, CompareOpToString(compareOp));
  return 1;
}

auto wrap_GetWidth(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  uint32_t width = texture->GetWidth();

  lua_pushinteger(state, static_cast<lua_Integer>(width));
  return 1;
}

auto wrap_GetHeight(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  uint32_t height = texture->GetHeight();

  lua_pushinteger(state, static_cast<lua_Integer>(height));
  return 1;
}

auto wrap_GetDepth(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  uint32_t depth = texture->GetDepth();

  lua_pushinteger(state, static_cast<lua_Integer>(depth));
  return 1;
}

auto wrap_GetDimensions(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  VkExtent2D dimensions = texture->GetDimensions();

  lua_pushinteger(state, static_cast<lua_Integer>(dimensions.width));
  lua_pushinteger(state, static_cast<lua_Integer>(dimensions.height));
  return 2;
}

auto wrap_GetMipmapCount(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  size_t mipmapCount = texture->GetMipmapCount();

  lua_pushinteger(state, static_cast<lua_Integer>(mipmapCount));
  return 1;
}

auto wrap_GetFormat(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  VkFormat format = texture->GetFormat();

  lua_pushstring(state, Format::ImageFormatToString(format).c_str());
  return 1;
}

enum class LuaTextureUsage : uint8_t {
  None = 0,
  Sampled = 1U << 0U,
  RenderTarget = 1U << 1U,
  Storage = 1U << 2U,
};

static inline auto TextureUsageToVkImageUsage(VkFormat format,
                                              LuaTextureUsage usage)
    -> VkImageUsageFlags {
  VkImageUsageFlags flags = 0;
  if ((static_cast<uint8_t>(usage) &
       static_cast<uint8_t>(LuaTextureUsage::Sampled)) != 0) {
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if ((static_cast<uint8_t>(usage) &
       static_cast<uint8_t>(LuaTextureUsage::RenderTarget)) != 0) {
    if (Image::IsDepthTexture(format) || Image::IsStencilTexture(format)) {
      flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    } else {
      flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
  }
  if ((static_cast<uint8_t>(usage) &
       static_cast<uint8_t>(LuaTextureUsage::Storage)) != 0) {
    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  return flags;
}

constexpr VkFormat DefaultFormat = VK_FORMAT_R8G8B8A8_UNORM;

const std::set<std::string> ValidOptionKeys = {
    "type",    "format",      "mipmaps",     "sampler", "rendertarget",
    "storage", "mipmapcount", "mipmapstart", "linear",
};

auto CheckTopOfStackTableKeys(lua_State *state, int index) -> Result<void> {
  luaL_checktype(state, index, LUA_TTABLE);

  lua_pushnil(state); // first key
  while (lua_next(state, index) != 0) {
    // key at -2, value at -1
    if (lua_type(state, -2) == LUA_TSTRING) {
      const char *keyStr = luaL_checkstring(state, -2);
      if (!ValidOptionKeys.contains(std::string(keyStr))) {
        return Error::Unexpected(
            std::format("Invalid texture option key: {}", keyStr));
      }
    }

    lua_pop(state, 1); // pop value, keep key for next iteration
  }

  return {};
}

// Options:
// { type = "2D"|"3D"|"array"|"cube", format = f, mipmaps = bool, usage = int, mipmapcount = n, mipmapstart = n, linear = bool }
struct LuaOptions {
  Graphics::Texture::TextureType type = Graphics::Texture::TextureType::DEFAULT;
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  bool mipmaps = false;
  LuaTextureUsage usage = LuaTextureUsage::Sampled;
  uint32_t mipmapCount = 0;
  uint32_t mipmapStart = 0;
  bool linear = true;

  static auto Create(lua_State *state, int index) -> Result<LuaOptions> {
    LuaOptions options{};
    luaL_checktype(state, index, LUA_TTABLE);

    auto result = CheckTopOfStackTableKeys(state, index);

    if (Error::IsError(result)) {
      return result.error().AsUnexpected();
    }

    // type
    lua_getfield(state, index, "type");
    if (lua_isstring(state, -1) != 0) {
      const char *typeStr = luaL_checkstring(state, -1);
      if (strcmp(typeStr, "2D") == 0) {
        options.type = Graphics::Texture::TextureType::DEFAULT;
      } else if (strcmp(typeStr, "3D") == 0) {
        options.type = Graphics::Texture::TextureType::VOLUME;
      } else if (strcmp(typeStr, "array") == 0) {
        options.type = Graphics::Texture::TextureType::ARRAY;
      } else if (strcmp(typeStr, "cube") == 0) {
        options.type = Graphics::Texture::TextureType::CUBEMAP;
      } else {
        return Error::Unexpected(
            std::format("Invalid texture type string: {}", typeStr));
      }
    }
    lua_pop(state, 1);

    // format
    lua_getfield(state, index, "format");
    if (lua_isstring(state, -1) != 0) {
      const char *formatStr = luaL_checkstring(state, -1);
      options.format = Format::StringToImageFormat(std::string(formatStr));
    }
    lua_pop(state, 1);

    // mipmaps
    lua_getfield(state, index, "mipmaps");
    if (lua_isboolean(state, -1) != 0) {
      options.mipmaps = (lua_toboolean(state, -1) != 0);
    }
    lua_pop(state, 1);

    LuaTextureUsage newUsage{};
    bool usageSet = false;

    // sampler
    lua_getfield(state, index, "sampler");
    if (lua_isboolean(state, -1) != 0) {
      if (lua_toboolean(state, -1) != 0) {
        newUsage = static_cast<LuaTextureUsage>(
            static_cast<uint8_t>(newUsage) |
            static_cast<uint8_t>(LuaTextureUsage::Sampled));
      }

      usageSet = true;
    }
    lua_pop(state, 1);

    // rendertarget
    lua_getfield(state, index, "rendertarget");
    if (lua_isboolean(state, -1) != 0) {
      if (lua_toboolean(state, -1) != 0) {
        newUsage = static_cast<LuaTextureUsage>(
            static_cast<uint8_t>(newUsage) |
            static_cast<uint8_t>(LuaTextureUsage::RenderTarget));
      }

      usageSet = true;
    }
    lua_pop(state, 1);

    // storage
    lua_getfield(state, index, "storage");
    if (lua_isboolean(state, -1) != 0) {
      if (lua_toboolean(state, -1) != 0) {
        newUsage = static_cast<LuaTextureUsage>(
            static_cast<uint8_t>(newUsage) |
            static_cast<uint8_t>(LuaTextureUsage::Storage));
      }

      usageSet = true;
    }
    lua_pop(state, 1);

    if (usageSet) {
      if (newUsage == LuaTextureUsage::None) {
        return Error::Unexpected("Texture usage cannot be none.");
      }

      PrintAlways("Setting texture usage to {}",
                  static_cast<uint8_t>(newUsage));

      options.usage = newUsage;
    }

    // mipmapcount
    lua_getfield(state, index, "mipmapcount");
    if (lua_isnumber(state, -1) != 0) {
      options.mipmapCount = static_cast<uint32_t>(lua_tointeger(state, -1));
    }
    lua_pop(state, 1);

    // mipmapstart
    lua_getfield(state, index, "mipmapstart");
    if (lua_isnumber(state, -1) != 0) {
      options.mipmapStart = static_cast<uint32_t>(lua_tointeger(state, -1));
    }
    lua_pop(state, 1);

    // linear
    lua_getfield(state, index, "linear");
    if (lua_isboolean(state, -1) != 0) {
      options.linear = (lua_toboolean(state, -1) != 0);
    }
    lua_pop(state, 1);

    return options;
  }
};

static inline auto TextureFromImagedata(lua_State *state)
    -> Result<Ref<Graphics::Texture::Texture>> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  auto *imageData = LuaWrap::ObjectFromLua<Image::ImageData>(state, 1);

  auto result = Graphics::Texture::LoadFromMemory(*ctx, *imageData,
                                                  VK_IMAGE_USAGE_SAMPLED_BIT);
  if (Error::IsError(result)) {
    return result.error().AsUnexpected();
  }
  return result.value();
}

static inline auto TextureFromFilepath(lua_State *state)
    -> Result<Ref<Graphics::Texture::Texture>> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  const char *filepath = luaL_checkstring(state, 1);

  auto result = Graphics::Texture::LoadFromFile(*ctx, filepath);
  if (Error::IsError(result)) {
    return result.error().AsUnexpected();
  }
  return result.value();
}

static inline auto TextureFromImagedataAndOptions(lua_State *state)
    -> Result<Ref<Graphics::Texture::Texture>> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  auto *imageData = LuaWrap::ObjectFromLua<Image::ImageData>(state, 1);
  auto optionsResult = LuaOptions::Create(state, 2);

  if (Error::IsError(optionsResult)) {
    return optionsResult.error().AsUnexpected();
  }

  LuaOptions options = optionsResult.value();

  auto usage = TextureUsageToVkImageUsage(options.format, options.usage);

  auto result = Graphics::Texture::LoadFromMemory(*ctx, *imageData, usage);
  if (Error::IsError(result)) {
    return result.error().AsUnexpected();
  }
  return result.value();
}

static inline auto TextureFromFilepathAndOptions(lua_State *state)
    -> Result<Ref<Graphics::Texture::Texture>> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  const char *filepath = luaL_checkstring(state, 1);
  auto optionsResult = LuaOptions::Create(state, 2);

  if (Error::IsError(optionsResult)) {
    return optionsResult.error().AsUnexpected();
  }

  LuaOptions options = optionsResult.value();

  auto usage = TextureUsageToVkImageUsage(options.format, options.usage);

  auto result = Graphics::Texture::LoadFromFile(*ctx, filepath, usage);
  if (Error::IsError(result)) {
    return result.error().AsUnexpected();
  }
  return result.value();
}

static inline auto TextureFromImagedataArrayAndOptions(lua_State *state)
    -> Result<Ref<Graphics::Texture::Texture>> {
  std::vector<Image::ImageData *> slices;
  size_t len = lua_objlen(state, 1);
  for (size_t i = 0; i < len; ++i) {
    lua_rawgeti(state, 1, static_cast<int>(i + 1));
    auto *imageData = LuaWrap::ObjectFromLua<Image::ImageData>(state, -1);
    slices.push_back(imageData);
    lua_pop(state, 1);
  }

  auto optionsResult = LuaOptions::Create(state, 2);

  if (Error::IsError(optionsResult)) {
    return optionsResult.error().AsUnexpected();
  }

  LuaOptions options = optionsResult.value();

  auto *ctx = Graphics::GetCurrentGraphicsContext();

  auto result = Graphics::Texture::LoadFromMemory(*ctx, slices, options.type);
  if (Error::IsError(result)) {
    return result.error().AsUnexpected();
  }
  return result.value();
}

static inline auto TextureFromWidthAndHeight(lua_State *state)
    -> Result<Ref<Graphics::Texture::Texture>> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  auto width = static_cast<uint32_t>(luaL_checkinteger(state, 1));
  auto height = static_cast<uint32_t>(luaL_checkinteger(state, 2));

  auto result = Graphics::Texture::Create2D(
      *ctx,
      Graphics::Texture::TextureCreationInfo{
          .width = width,
          .height = height,
          .format = VK_FORMAT_R8G8B8A8_UNORM,
          .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          .mipmapCount = 1,
      });
  if (Error::IsError(result)) {
    return result.error().AsUnexpected();
  }
  return result.value();
}

static inline auto TextureFromWidthHeightAndOptions(lua_State *state)
    -> Result<Ref<Graphics::Texture::Texture>> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  auto width = static_cast<uint32_t>(luaL_checkinteger(state, 1));
  auto height = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  auto optionsResult = LuaOptions::Create(state, 3);

  if (Error::IsError(optionsResult)) {
    return optionsResult.error().AsUnexpected();
  }

  LuaOptions options = optionsResult.value();

  auto usage = TextureUsageToVkImageUsage(options.format, options.usage);

  auto result = Graphics::Texture::Create2D(
      *ctx, Graphics::Texture::TextureCreationInfo{
                .width = width,
                .height = height,
                .format = options.format,
                .usage = usage,
                .mipmapCount = options.mipmaps ? 0 : 1,
            });
  if (Error::IsError(result)) {
    return result.error().AsUnexpected();
  }
  return result.value();
}

static inline auto
TextureFromWidthHeightDepthOrLayersAndOptions(lua_State *state)
    -> Result<Ref<Graphics::Texture::Texture>> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  auto width = static_cast<uint32_t>(luaL_checkinteger(state, 1));
  auto height = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  auto depthOrLayers = static_cast<uint32_t>(luaL_checkinteger(state, 3));
  auto optionsResult = LuaOptions::Create(state, 4);

  if (Error::IsError(optionsResult)) {
    return optionsResult.error().AsUnexpected();
  }

  LuaOptions options = optionsResult.value();

  auto usage = TextureUsageToVkImageUsage(options.format, options.usage);

  Result<Ref<Graphics::Texture::Texture>> result;
  switch (options.type) {
  case Graphics::Texture::TextureType::VOLUME:
    result = Graphics::Texture::CreateVolume(
        *ctx, Graphics::Texture::TextureCreationInfo{
                  .width = width,
                  .height = height,
                  .depth = depthOrLayers,
                  .format = options.format,
                  .usage = usage,
                  .mipmapCount = options.mipmaps ? 0 : 1,
              });
    break;
  case Graphics::Texture::TextureType::ARRAY:
    result = Graphics::Texture::CreateArray(
        *ctx, Graphics::Texture::TextureCreationInfo{
                  .width = width,
                  .height = height,
                  .depth = depthOrLayers,
                  .format = options.format,
                  .usage = usage,
                  .mipmapCount = options.mipmaps ? 0 : 1,
              });
    break;
  default:
    return Error::Unexpected(
        "Invalid texture type for 3D/Array texture creation.");
  }

  if (Error::IsError(result)) {
    return result.error().AsUnexpected();
  }
  return result.value();
}

// Variants:
// Filepath -> load from file
// Imagedata -> load from image data
// Filepath, Options -> load from file with options
// Imagedata, Options -> load from image data with options
// Array of Imagedata, Options -> load 3D/Array/Cubemap texture
// width, height -> 2D rgba8 1 mip, 1 layer texture
// width, height, Options -> 2D or Cubemap texture
// width, height, depth|layers, Options, -> 3D or Array texture
auto wrap_NewTexture(lua_State *state) -> int {
  auto *ctx = Graphics::GetCurrentGraphicsContext();

  const auto *type = Graphics::Texture::Texture::GetType();
  int args = lua_gettop(state);

  Result<Ref<Graphics::Texture::Texture>> result;

  if (args == 1) {
    if (LuaWrap::IsType<Image::ImageData>(state, 1)) {
      PrintAlways("Creating texture from imagedata");
      result = TextureFromImagedata(state);
    } else if (lua_type(state, 1) == LUA_TSTRING) { // Filepath
      PrintAlways("Creating texture from filepath");
      result = TextureFromFilepath(state);
    } else {
      return luaL_error(state, "Invalid argument to newTexture");
    }
  } else if (args == 2) {
    // Width, Height or ImageData array + Options or Filepath + Options or ImageData + Options
    if (LuaWrap::IsType<Image::ImageData>(state, 1)) {
      PrintAlways("Creating texture from imagedata and options");
      result = TextureFromImagedataAndOptions(state);
    } else if (lua_type(state, 1) == LUA_TSTRING) { // Filepath + Options
      PrintAlways("Creating texture from filepath and options");
      result = TextureFromFilepathAndOptions(state);
    } else if (lua_istable(state, 1) != 0 &&
               lua_istable(state, 2) != 0) { // ImageData array + Options
      PrintAlways("Creating texture from imagedata array and options");
      result = TextureFromImagedataArrayAndOptions(state);
    } else { // Width, Height
      PrintAlways("Creating texture from width and height");
      result = TextureFromWidthAndHeight(state);
    }
  } else if (args == 3) { // width, height, Options
    PrintAlways("Creating texture from width, height and options");
    result = TextureFromWidthHeightAndOptions(state);
  } else if (args == 4) { // width, height, depth|layers, Options
    PrintAlways(
        "Creating texture from width, height, depth/layers and options");
    result = TextureFromWidthHeightDepthOrLayersAndOptions(state);
  } else {
    return luaL_error(state, "Invalid arguments to newTexture");
  }

  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to create texture: %s",
                      result.error().message.c_str());
  }
  auto texture = result.value();

  LuaWrap::PushObject(state, type, texture.get());
  texture->release(); // Retained by lua now

  return 1;
}

auto wrap_Release(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Texture>(state, 1);

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  texture->ScheduleDestroy();
  return 0;
}

} // namespace Graphics::Texture