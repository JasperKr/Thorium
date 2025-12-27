#include "wrap_imagedata.hpp"
#include "Graphics/format.hpp"
#include "Modules/imagedata.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>

namespace Image {

// width: number, height: number
// width: number, height: number, format: string
// bytedata: Bytedata (load as filedata)
// filepath: string
auto wrap_NewImagedata(lua_State *state) -> int {
  if (lua_isnumber(state, 1) != 0 && lua_isnumber(state, 2) != 0) {
    // Create new empty imagedata
    auto width = static_cast<size_t>(luaL_checkinteger(state, 1));
    auto height = static_cast<size_t>(luaL_checkinteger(state, 2));

    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

    if (lua_isstring(state, 3) != 0) {
      const auto *formatStr = luaL_checkstring(state, 3);
      format = Graphics::Format::StringToImageFormat(formatStr);
    }

    auto imagedata = Image::ImageData::Create(
        static_cast<uint32_t>(width), static_cast<uint32_t>(height), format);

    LuaWrap::PushLuaType(state, Image::ImageData::GetType(), imagedata);
    imagedata->release(); // Release C++ reference, Lua now owns it
  } else if (lua_isstring(state, 1) != 0) {
    const auto *filepath = luaL_checkstring(state, 1);
    auto imagedata = Image::ImageData::Create(filepath);

    LuaWrap::PushLuaType(state, Image::ImageData::GetType(), imagedata);
    imagedata->release(); // Release C++ reference, Lua now owns it
  } else {
    return luaL_error(state, "Invalid arguments to Imagedata constructor.");
  }
}

} // namespace Image