#include "Graphics/texture.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/image.hpp"
#include "Modules/imagedata.hpp"
#include "Modules/object.hpp"
#include "Wrap/wrap.hpp"
#include <cstdint>
#include <lauxlib.h>
#include <lua.h>

namespace WrapTemplate {

enum class TextureUsage : uint8_t {
  Sampled = 1U << 0U,
  RenderTarget = 1U << 1U,
  Storage = 1U << 2U,
};

static inline auto TextureUsageToVkImageUsage(VkFormat format,
                                              TextureUsage usage)
    -> VkImageUsageFlags {
  VkImageUsageFlags flags = 0;
  if ((static_cast<uint8_t>(usage) &
       static_cast<uint8_t>(TextureUsage::Sampled)) != 0) {
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if ((static_cast<uint8_t>(usage) &
       static_cast<uint8_t>(TextureUsage::RenderTarget)) != 0) {
    if (Image::IsDepthTexture(format)) {
      flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    } else if (Image::IsStencilTexture(format)) {
      flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    } else {
      flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
  }
  if ((static_cast<uint8_t>(usage) &
       static_cast<uint8_t>(TextureUsage::Storage)) != 0) {
    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  return flags;
}

constexpr VkFormat DefaultFormat = VK_FORMAT_R8G8B8A8_UNORM;

// Options:
// { type = "2D"|"3D"|"array"|"cube", format = f, mipmaps = bool, usage = int, mipmapcount = n, mipmapstart = n, linear = bool }
struct LuaOptions {
  Graphics::Texture::TextureType type = Graphics::Texture::TextureType::DEFAULT;
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  bool mipmaps = false;
  TextureUsage usage = TextureUsage::Sampled;
  uint32_t mipmapCount = 0;
  uint32_t mipmapStart = 0;
  bool linear = true;

  LuaOptions(lua_State *state, int index) {
    luaL_checktype(state, index, LUA_TTABLE);

    // type
    lua_getfield(state, index, "type");
    if (lua_isstring(state, -1) != 0) {
      const char *typeStr = luaL_checkstring(state, -1);
      if (strcmp(typeStr, "2D") == 0) {
        this->type = Graphics::Texture::TextureType::DEFAULT;
      } else if (strcmp(typeStr, "3D") == 0) {
        this->type = Graphics::Texture::TextureType::VOLUME;
      } else if (strcmp(typeStr, "array") == 0) {
        this->type = Graphics::Texture::TextureType::ARRAY;
      } else if (strcmp(typeStr, "cube") == 0) {
        this->type = Graphics::Texture::TextureType::CUBEMAP;
      }
    }
    lua_pop(state, 1);

    // format
    lua_getfield(state, index, "format");
    if (lua_isstring(state, -1) != 0) {
      const char *formatStr = luaL_checkstring(state, -1);
      this->format = Image::StringToFormat(std::string(formatStr));
    }
    lua_pop(state, 1);

    // mipmaps
    lua_getfield(state, index, "mipmaps");
    if (lua_isboolean(state, -1) != 0) {
      this->mipmaps = (lua_toboolean(state, -1) != 0);
    }
    lua_pop(state, 1);

    // usage
    lua_getfield(state, index, "usage");
    if (lua_isnumber(state, -1) != 0) {
      int usageInt = static_cast<int>(lua_tointeger(state, -1));
      this->usage = static_cast<TextureUsage>(usageInt);
    }
    lua_pop(state, 1);

    // mipmapcount
    lua_getfield(state, index, "mipmapcount");
    if (lua_isnumber(state, -1) != 0) {
      this->mipmapCount = static_cast<uint32_t>(lua_tointeger(state, -1));
    }
    lua_pop(state, 1);

    // mipmapstart
    lua_getfield(state, index, "mipmapstart");
    if (lua_isnumber(state, -1) != 0) {
      this->mipmapStart = static_cast<uint32_t>(lua_tointeger(state, -1));
    }
    lua_pop(state, 1);

    // linear
    lua_getfield(state, index, "linear");
    if (lua_isboolean(state, -1) != 0) {
      this->linear = (lua_toboolean(state, -1) != 0);
    }
    lua_pop(state, 1);
  }
};

static inline auto TextureFromImagedata(lua_State *state)
    -> tl::expected<Ref<Graphics::Texture::Texture>, Error::Error> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  auto *imageData = LuaWrap::FromLuaObject<Image::ImageData>(state, 1);

  auto result = Graphics::Texture::LoadFromMemory(*ctx, *imageData);
  if (Error::IsError(result)) {
    return tl::unexpected(result.error());
  }
  return result.value();
}

static inline auto TextureFromFilepath(lua_State *state)
    -> tl::expected<Ref<Graphics::Texture::Texture>, Error::Error> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  const char *filepath = luaL_checkstring(state, 1);

  auto result = Graphics::Texture::LoadFromFile(*ctx, filepath);
  if (Error::IsError(result)) {
    return tl::unexpected(result.error());
  }
  return result.value();
}

static inline auto TextureFromImagedataAndOptions(lua_State *state)
    -> tl::expected<Ref<Graphics::Texture::Texture>, Error::Error> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  auto *imageData = LuaWrap::FromLuaObject<Image::ImageData>(state, 1);
  LuaOptions options(state, 2);

  auto usage = TextureUsageToVkImageUsage(options.format, options.usage);

  auto result = Graphics::Texture::LoadFromMemory(*ctx, *imageData, usage);
  if (Error::IsError(result)) {
    return tl::unexpected(result.error());
  }
  return result.value();
}

static inline auto TextureFromFilepathAndOptions(lua_State *state)
    -> tl::expected<Ref<Graphics::Texture::Texture>, Error::Error> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  const char *filepath = luaL_checkstring(state, 1);
  LuaOptions options(state, 2);

  auto usage = TextureUsageToVkImageUsage(options.format, options.usage);

  auto result = Graphics::Texture::LoadFromFile(*ctx, filepath, usage);
  if (Error::IsError(result)) {
    return tl::unexpected(result.error());
  }
  return result.value();
}

static inline auto TextureFromImagedataArrayAndOptions(lua_State *state)
    -> tl::expected<Ref<Graphics::Texture::Texture>, Error::Error> {
  std::vector<Image::ImageData *> slices;
  size_t len = lua_objlen(state, 1);
  for (size_t i = 0; i < len; ++i) {
    lua_rawgeti(state, 1, static_cast<int>(i + 1));
    auto *imageData = LuaWrap::FromLuaObject<Image::ImageData>(state, -1);
    slices.push_back(imageData);
    lua_pop(state, 1);
  }

  // Options
  LuaOptions options(state, 2);

  auto *ctx = Graphics::GetCurrentGraphicsContext();

  auto result = Graphics::Texture::LoadFromMemory(*ctx, slices, options.type);
  if (Error::IsError(result)) {
    return tl::unexpected(result.error());
  }
  return result.value();
}

static inline auto TextureFromWidthAndHeight(lua_State *state)
    -> tl::expected<Ref<Graphics::Texture::Texture>, Error::Error> {
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
    return tl::unexpected(result.error());
  }
  return result.value();
}

static inline auto TextureFromWidthHeightAndOptions(lua_State *state)
    -> tl::expected<Ref<Graphics::Texture::Texture>, Error::Error> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  auto width = static_cast<uint32_t>(luaL_checkinteger(state, 1));
  auto height = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  LuaOptions options(state, 3);

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
    return tl::unexpected(result.error());
  }
  return result.value();
}

static inline auto
TextureFromWidthHeightDepthOrLayersAndOptions(lua_State *state)
    -> tl::expected<Ref<Graphics::Texture::Texture>, Error::Error> {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  auto width = static_cast<uint32_t>(luaL_checkinteger(state, 1));
  auto height = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  auto depthOrLayers = static_cast<uint32_t>(luaL_checkinteger(state, 3));
  LuaOptions options(state, 4);

  auto usage = TextureUsageToVkImageUsage(options.format, options.usage);

  tl::expected<Ref<Graphics::Texture::Texture>, Error::Error> result;
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
    return tl::unexpected(
        Error::Create("Invalid texture type for 3D/Array texture creation."));
  }

  if (Error::IsError(result)) {
    return tl::unexpected(result.error());
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

  auto type = *Graphics::Texture::Texture::GetType();
  int args = lua_gettop(state);

  Ref<Graphics::Texture::Texture> texture;
  tl::expected<Ref<Graphics::Texture::Texture>, Error::Error> result;

  if (args == 1) {
    if (LuaWrap::LuaIsType<Image::ImageData>(state, 1)) {
      result = TextureFromImagedata(state);
    } else if (lua_isstring(state, 1) != 0) {
      result = TextureFromFilepath(state);
    } else {
      return luaL_error(state, "Invalid argument to newTexture");
    }
  } else if (args == 2) {
    // Width, Height or ImageData array + Options or Filepath + Options or ImageData + Options
    if (LuaWrap::LuaIsType<Image::ImageData>(state, 1)) {
      result = TextureFromImagedataAndOptions(state);
    } else if (lua_isstring(state, 1) != 0) {
      result = TextureFromFilepathAndOptions(state);
    } else if (lua_istable(state, 1) != 0 && lua_istable(state, 2) != 0) { // ImageData array + Options
      result = TextureFromImagedataArrayAndOptions(state);
    } else { // Width, Height
      result = TextureFromWidthAndHeight(state);
    }
  } else if (args == 3) { // width, height, Options
    result = TextureFromWidthHeightAndOptions(state);
  } else if (args == 4) { // width, height, depth|layers, Options
    result = TextureFromWidthHeightDepthOrLayersAndOptions(state);
  } else {
    return luaL_error(state, "Invalid arguments to newTexture");
  }

  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to create texture: %s",
                      result.error().message.c_str());
  }
  texture = result.value();

  return 1;
}

} // namespace WrapTemplate