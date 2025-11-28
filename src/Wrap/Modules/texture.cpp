#include "Graphics/texture.hpp"
#include "Modules/object.hpp"
#include <cstdint>
#include <lauxlib.h>
#include <lua.h>

namespace WrapTemplate {

enum class TextureUsage : uint8_t {
  Sampled = 1U << 0U,
  RenderTarget = 1U << 1U,
  Storage = 1U << 2U,
};

struct TextureOptions {
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  TextureUsage usage = TextureUsage::Sampled;
  uint32_t mipmapStart = 0;
  uint32_t mipmapCount = 1;
  bool linear = true;
};

constexpr VkFormat DefaultFormat = VK_FORMAT_R8G8B8A8_UNORM;

// NOLINTNEXTLINE
auto inline StringToFormat(std::string &format) -> VkFormat {
  // 8-bit unorm
  if (format == "rgba8") {
    return VK_FORMAT_R8G8B8A8_UNORM;
  }
  if (format == "rg8") {
    return VK_FORMAT_R8G8_UNORM;
  }
  if (format == "r8") {
    return VK_FORMAT_R8_UNORM;
  }
  // 16-bit unorm
  if (format == "rgba16") {
    return VK_FORMAT_R16G16B16A16_UNORM;
  }
  if (format == "rg16") {
    return VK_FORMAT_R16G16_UNORM;
  }
  if (format == "r16") {
    return VK_FORMAT_R16_UNORM;
  }
  // 16-bit float
  if (format == "rgba16f") {
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  }
  if (format == "rg16f") {
    return VK_FORMAT_R16G16_SFLOAT;
  }
  if (format == "r16f") {
    return VK_FORMAT_R16_SFLOAT;
  }
  // 32-bit float
  if (format == "rgba32f") {
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  }
  if (format == "rg32f") {
    return VK_FORMAT_R32G32_SFLOAT;
  }
  if (format == "r32f") {
    return VK_FORMAT_R32_SFLOAT;
  }
  // 8-bit uint
  if (format == "rgba8ui") {
    return VK_FORMAT_R8G8B8A8_UINT;
  }
  if (format == "rg8ui") {
    return VK_FORMAT_R8G8_UINT;
  }
  if (format == "r8ui") {
    return VK_FORMAT_R8_UINT;
  }
  // 16-bit uint
  if (format == "rgba16ui") {
    return VK_FORMAT_R16G16B16A16_UINT;
  }
  if (format == "rg16ui") {
    return VK_FORMAT_R16G16_UINT;
  }
  if (format == "r16ui") {
    return VK_FORMAT_R16_UINT;
  }
  // 32-bit uint
  if (format == "rgba32ui") {
    return VK_FORMAT_R32G32B32A32_UINT;
  }
  if (format == "rg32ui") {
    return VK_FORMAT_R32G32_UINT;
  }
  if (format == "r32ui") {
    return VK_FORMAT_R32_UINT;
  }
  // 8-bit sint
  if (format == "rgba8si") {
    return VK_FORMAT_R8G8B8A8_SINT;
  }
  if (format == "rg8si") {
    return VK_FORMAT_R8G8_SINT;
  }
  if (format == "r8si") {
    return VK_FORMAT_R8_SINT;
  }
  // 16-bit sint
  if (format == "rgba16si") {
    return VK_FORMAT_R16G16B16A16_SINT;
  }
  if (format == "rg16si") {
    return VK_FORMAT_R16G16_SINT;
  }
  if (format == "r16si") {
    return VK_FORMAT_R16_SINT;
  }
  // 32-bit sint
  if (format == "rgba32si") {
    return VK_FORMAT_R32G32B32A32_SINT;
  }
  if (format == "rg32si") {
    return VK_FORMAT_R32G32_SINT;
  }
  if (format == "r32si") {
    return VK_FORMAT_R32_SINT;
  }
  // Depth formats
  if (format == "depth16") {
    return VK_FORMAT_D16_UNORM;
  }
  if (format == "depth24") {
    return VK_FORMAT_X8_D24_UNORM_PACK32;
  }
  if (format == "depth32") {
    return VK_FORMAT_D32_SFLOAT;
  }
  // Depth-stencil formats
  if (format == "depth24stencil8") {
    return VK_FORMAT_D24_UNORM_S8_UINT;
  }
  if (format == "depth32stencil8") {
    return VK_FORMAT_D32_SFLOAT_S8_UINT;
  }
  // packed formats
  if (format == "rg11b10f") {
    return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
  }
  if (format == "rgb9e5") {
    return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
  }
  if (format == "rgb10a2") {
    return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
  }
  if (format == "rgb10a2ui") {
    return VK_FORMAT_A2B10G10R10_UINT_PACK32;
  }
  if (format == "bgr5a1") {
    return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
  }
  if (format == "bgr565") {
    return VK_FORMAT_R5G6B5_UNORM_PACK16;
  }
  if (format == "rgba4") {
    return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
  }
  // compressed formats
  if (format == "bc1") {
    return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
  }
  if (format == "bc3") {
    return VK_FORMAT_BC3_UNORM_BLOCK;
  }
  if (format == "bc4") {
    return VK_FORMAT_BC4_UNORM_BLOCK;
  }
  if (format == "bc5") {
    return VK_FORMAT_BC5_UNORM_BLOCK;
  }
  if (format == "bc6h") {
    return VK_FORMAT_BC6H_UFLOAT_BLOCK;
  }
  if (format == "bc6hs") {
    return VK_FORMAT_BC6H_SFLOAT_BLOCK;
  }
  if (format == "bc7") {
    return VK_FORMAT_BC7_UNORM_BLOCK;
  }

  return VK_FORMAT_UNDEFINED;
}

auto inline TextureOptionsFromLuaState(lua_State *state, int index)
    -> TextureOptions {
  TextureOptions options = {};

  lua_getfield(state, index, "format");
  if (lua_isstring(state, -1) != 0) {
    std::string formatStr = lua_tostring(state, -1);
    options.format = StringToFormat(formatStr);
  }
  lua_pop(state, 1);

  lua_getfield(state, index, "usage");
  if (lua_isnumber(state, -1) != 0) {
    options.usage = static_cast<TextureUsage>(lua_tointeger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, index, "mipmapstart");
  if (lua_isnumber(state, -1) != 0) {
    options.mipmapStart = static_cast<uint32_t>(lua_tointeger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, index, "mipmapcount");
  if (lua_isnumber(state, -1) != 0) {
    options.mipmapCount = static_cast<uint32_t>(lua_tointeger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, index, "linear");
  if (lua_isboolean(state, -1)) {
    options.linear = (lua_toboolean(state, -1) != 0);
  }
  lua_pop(state, 1);

  return options;
}

// 1: width, height, {format = "rgba8", usage = enum, mipmapstart = 0, mipmapcount = 1, linear = true}
// 2: filepath, {usage = enum, mipmapstart = 0, mipmapcount = 1, linear = true}
// 3: imagedata, {usage = enum, mipmapstart = 0, mipmapcount = 1, linear = true}
// returns: texture wrapper
auto wrap_newTexture(lua_State *state) -> int {
  auto argCount = lua_gettop(state);
  bool createdFromSize =
      (lua_isnumber(state, 1) != 0) && (lua_isnumber(state, 2) != 0);
  bool createdFromFilepath = (lua_isstring(state, 1) != 0);
  bool createdFromImageData =
      (lua_islightuserdata(state, 1) != 0); // TODO: check type
  if (!createdFromSize && !createdFromFilepath && !createdFromImageData) {
    return luaL_error(
        state,
        "Invalid arguments to Texture.new. Expected (width: number, "
        "height: number, [options: table]) or (filepath: string, [options: "
        "table]) or (imagedata: userdata, [options: table])");
  }

  auto *context = Graphics::GetCurrentGraphicsContext();
  Graphics::Texture::Texture texture{};

  if (createdFromSize) {
    // Create texture from size
    auto width = static_cast<uint32_t>(lua_tointeger(state, 1));
    auto height = static_cast<uint32_t>(lua_tointeger(state, 2));
    bool hasOptions = lua_istable(state, 3);

    TextureOptions options = {};

    if (hasOptions) {
      options = TextureOptionsFromLuaState(state, 3);
    }

    Graphics::Texture::TextureCreationInfo info = {
        .width = width, .height = height, .format = options.format};

    auto result = Graphics::Texture::Create2D(*context, info);

    if (Error::IsError(result)) {
      return luaL_error(state, "Failed to create texture: %s",
                        result.error().ToString().c_str());
    }
    texture = result.value();
  }

  // auto *proxy = new Proxy(texture);

  // lua_pushlightuserdata(state, proxy);

  return 1; // Number of return values
}

} // namespace WrapTemplate