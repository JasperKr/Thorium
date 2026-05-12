#include "wrap_imagedata.hpp"
#include "Graphics/format.hpp"
#include "Graphics/graphicsState.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/color.hpp"
#include "Modules/imagedata.hpp"
#include "Wrap/wrap.hpp"
#include <lua.hpp>

#include <vulkan/vulkan_core.h>

namespace Wrap::Image {

// width: number, height: number
// width: number, height: number, format: string
// bytedata: Bytedata (load as filedata)
// filepath: string
auto wrap_NewImagedata(lua_State *state) -> int {
  if (lua_isnumber(state, 1) != 0 && lua_isnumber(state, 2) != 0) {
    // Create new empty imagedata
    auto width = static_cast<size_t>(luaL_checkinteger(state, 1));
    auto height = static_cast<size_t>(luaL_checkinteger(state, 2));

    VkFormat format = Graphics::DefaultPixelFormat;

    if (lua_isstring(state, 3) != 0) {
      const auto *formatStr = luaL_checkstring(state, 3);
      format = Graphics::Format::StringToImageFormat(formatStr);
    }

    auto imagedataResult = ::Image::ImageData::Create(
        static_cast<uint32_t>(width), static_cast<uint32_t>(height), format);

    if (Error::IsError(imagedataResult)) {
      return luaL_error(state, "Failed to create ImageData: %s",
                        imagedataResult.error().message.c_str());
    }

    auto imagedata = imagedataResult.value();

    LuaWrap::PushObject(state, ::Image::ImageData::GetType(), imagedata.get());
  } else if (lua_isstring(state, 1) != 0) {
    const auto *filepath = luaL_checkstring(state, 1);
    auto imagedataResult = ::Image::ImageData::Create(filepath);

    if (Error::IsError(imagedataResult)) {
      return luaL_error(state, "Failed to create ImageData from file: %s",
                        imagedataResult.error().message.c_str());
    }

    auto imagedata = imagedataResult.value();

    LuaWrap::PushObject(state, ::Image::ImageData::GetType(), imagedata.get());
  } else {
    return luaL_error(state, "Invalid arguments to Imagedata constructor.");
  }

  return 1;
}

auto wrap_SetPixel(lua_State *state) -> int {
  auto x_pos = static_cast<size_t>(luaL_checkinteger(state, 2));
  auto y_pos = static_cast<size_t>(luaL_checkinteger(state, 3));

  auto r = luaL_checknumber(state, 4); // NOLINT
  auto g = luaL_checknumber(state, 5); // NOLINT
  auto b = luaL_checknumber(state, 6); // NOLINT
  auto a = luaL_checknumber(state, 7); // NOLINT

  auto *imagedata = LuaWrap::ObjectFromLua<::Image::ImageData>(state, 1);
  if (imagedata == nullptr) {
    return luaL_error(state, "Expected Imagedata object as first argument.");
  }

  Color color(r, g, b, a);
  Math::Uvec2 position(x_pos, y_pos);

  auto error = imagedata->SetColor(position, color);
  if (Error::IsError(error)) {
    return luaL_error(state, "Failed to set pixel color: %s",
                      error.message.c_str());
  }

  return 0;
}
auto wrap_GetPixel(lua_State *state) -> int {
  auto x_pos = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  auto y_pos = static_cast<uint32_t>(luaL_checkinteger(state, 3));

  auto *imagedata = LuaWrap::ObjectFromLua<::Image::ImageData>(state, 1);

  if (imagedata == nullptr) {
    return luaL_error(state, "Expected Imagedata object as first argument.");
  }

  auto result = imagedata->GetColor({x_pos, y_pos});
  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to get pixel color: %s",
                      result.error().message.c_str());
  }

  auto [r, g, b, a] = result.value();

  lua_pushnumber(state, r);
  lua_pushnumber(state, g);
  lua_pushnumber(state, b);
  lua_pushnumber(state, a);

  return 4;
}
auto wrap_GetWidth(lua_State *state) -> int {
  auto *imagedata = LuaWrap::ObjectFromLua<::Image::ImageData>(state, 1);

  if (imagedata == nullptr) {
    return luaL_error(state, "Expected Imagedata object as first argument.");
  }

  lua_pushinteger(state, imagedata->GetWidth());

  return 1;
}
auto wrap_GetHeight(lua_State *state) -> int {
  auto *imagedata = LuaWrap::ObjectFromLua<::Image::ImageData>(state, 1);

  if (imagedata == nullptr) {
    return luaL_error(state, "Expected Imagedata object as first argument.");
  }

  lua_pushinteger(state, imagedata->GetHeight());

  return 1;
}
auto wrap_GetFormat(lua_State *state) -> int {
  auto *imagedata = LuaWrap::ObjectFromLua<::Image::ImageData>(state, 1);

  if (imagedata == nullptr) {
    return luaL_error(state, "Expected Imagedata object as first argument.");
  }

  auto format = imagedata->GetFormat();
  auto formatStr = Graphics::Format::ImageFormatToString(format);

  lua_pushstring(state, formatStr.data());

  return 1;
}
auto wrap_GetDimensions(lua_State *state) -> int {
  auto *imagedata = LuaWrap::ObjectFromLua<::Image::ImageData>(state, 1);

  if (imagedata == nullptr) {
    return luaL_error(state, "Expected Imagedata object as first argument.");
  }

  auto width = imagedata->GetWidth();
  auto height = imagedata->GetHeight();

  lua_pushinteger(state, width);
  lua_pushinteger(state, height);

  return 2;
}

} // namespace Wrap::Image