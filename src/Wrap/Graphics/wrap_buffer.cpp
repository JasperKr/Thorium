#include "Wrap/Graphics/wrap_buffer.hpp"
#include "Graphics/barrier.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Wrap/Graphics/wrap_format.hpp"
#include "Wrap/Graphics/wrap_reflection.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_utils.hpp"
#include <cstdint>
#include <variant>
#include <vulkan/vulkan_core.h>
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#include <vector>

#include "Graphics/Buffers/structured.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/format.hpp"
#include "Modules/bytedata.hpp"

namespace Graphics {

/*
auto wrap_GetSize(lua_State *state) -> int;
auto wrap_GetElementCount(lua_State *state) -> int;
auto wrap_GetElementStride(lua_State *state) -> int;
auto wrap_Clear(lua_State *state) -> int;
auto wrap_GetFormat(lua_State *state) -> int;
auto wrap_SetData(lua_State *state) -> int;

auto wrap_NewBuffer(lua_State *state) -> int;
*/

auto wrap_GetSize(lua_State *state) -> int {
  auto *buffer = LuaWrap::ObjectFromLua<Graphics::StructuredBuffer>(state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  lua_pushinteger(state, static_cast<lua_Integer>(buffer->GetSize()));
  return 1;
}

auto wrap_GetElementCount(lua_State *state) -> int {
  auto *buffer = LuaWrap::ObjectFromLua<Graphics::StructuredBuffer>(state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  lua_pushinteger(state, static_cast<lua_Integer>(buffer->GetElementCount()));
  return 1;
}

auto wrap_GetElementStride(lua_State *state) -> int {
  auto *buffer = LuaWrap::ObjectFromLua<Graphics::StructuredBuffer>(state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  lua_pushinteger(state, static_cast<lua_Integer>(buffer->GetElementStride()));
  return 1;
}

// [value = 0], [offset = 0], [size = whole size]
auto wrap_ClearBuffer(lua_State *state) -> int {
  auto *buffer = LuaWrap::ObjectFromLua<Graphics::StructuredBuffer>(state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  uint32_t value = 0;
  if (lua_gettop(state) >= 2) {
    value = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  }

  VkDeviceSize offset = 0;
  if (lua_gettop(state) >= 3) {
    offset = static_cast<VkDeviceSize>(luaL_checkinteger(state, 3));
  }

  VkDeviceSize size = VK_WHOLE_SIZE;
  if (lua_gettop(state) >= 4) {
    size = static_cast<VkDeviceSize>(luaL_checkinteger(state, 4));
  }

  auto *ctx = GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current GraphicsContext set for this thread.");
  }

  auto clearResult = buffer->GetBuffer()->Clear(*ctx, value, offset, size);

  if (Error::IsError(clearResult)) {
    return luaL_error(state, "Failed to clear buffer: %s",
                      clearResult.message.c_str());
  }

  return 0;
}

inline auto BuildBufferFormatTree(lua_State *state, const BufferFormat &format)
    -> void {
  lua_newtable(state);

  int tableIndex = 1;
  for (const auto &component : format.GetComponents()) {
    lua_newtable(state);
    // name, list of strings
    lua_pushstring(state, "name");
    lua_newtable(state);

    lua_pushstring(state, component.name.c_str());
    lua_settable(state, -3); // table["name"] = name table

    // offset
    lua_pushstring(state, "offset");
    lua_pushinteger(state, static_cast<lua_Integer>(component.offset));
    lua_settable(state, -3); // table["offset"] = offset

    // format
    lua_pushstring(state, "format");
    if (std::holds_alternative<VkFormat>(component.format)) {
      auto vkFormat = std::get<VkFormat>(component.format);
      lua_pushstring(state, Format::ToString(vkFormat).c_str());
    } else if (std::holds_alternative<BufferFormat>(component.format)) {
      auto bufferFormat = std::get<BufferFormat>(component.format);
      BuildBufferFormatTree(state, bufferFormat);
    }
    lua_settable(state, -3); // table["format"] = format

    lua_pushinteger(state, tableIndex++);
    lua_insert(state, -2);   // move index below table
    lua_settable(state, -3); // main table[index] = component table
  }
}

// returns:
// { { name = ..., offset = ..., format = ... } }
auto wrap_GetFormat(lua_State *state) -> int {
  auto *buffer = LuaWrap::ObjectFromLua<Graphics::StructuredBuffer>(state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  auto format = buffer->GetFormat();

  BuildBufferFormatTree(state, format);

  return 1;
}

// data: Bytedata | table of numbers, srcOffset: integer, dstOffset: integer, size: integer
// So, stack: [buffer, data, srcOffset?, dstOffset?, size?]
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto wrap_SetData(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current GraphicsContext set for this thread.");
  }

  auto *buffer = LuaWrap::ObjectFromLua<Graphics::StructuredBuffer>(state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  std::vector<uint8_t> data{};

  Graphics::Barrier::UpdateUsage(*ctx, *buffer->GetBuffer(),
                                 Graphics::Barrier::ResourceState{
                                     .stages = VK_PIPELINE_STAGE_2_HOST_BIT,
                                     .access = VK_ACCESS_2_HOST_WRITE_BIT,
                                 });

  if (lua_istable(state, 2)) {
    // table of numbers

    auto sourceOffset = 1;

    if (lua_isnumber(state, 3) != 0) {
      sourceOffset = static_cast<int>(luaL_checkinteger(state, 3));
      if (sourceOffset < 1) {
        return luaL_error(state,
                          "Source offset cannot be less than 1 for table data");
      }
      if (static_cast<size_t>(sourceOffset) > buffer->GetSize()) {
        return luaL_error(state, "Source offset is out of bounds");
      }
    }

    size_t tableSize = lua_objlen(state, 2);
    data.resize(tableSize * sizeof(float));
    for (int i = 0; i < tableSize; ++i) {
      lua_rawgeti(state, 2, i + sourceOffset);
      auto value = luaL_checknumber(state, -1);

      auto result = // NOLINTNEXTLINE; pointer arithmetic
          Wrap::Utils::SetData(value, data.data() + (i * sizeof(float)),
                               buffer->GetFormat().FormatAt(i));

      if (Error::IsError(result)) {
        return luaL_error(state, "Failed to set buffer data: %s",
                          result.message.c_str());
      }

      lua_pop(state, 1);
    }
  } else {
    auto sourceOffset = 0;

    if (lua_isnumber(state, 3) != 0) {
      sourceOffset = static_cast<int>(luaL_checkinteger(state, 3));
      if (sourceOffset < 0) {
        return luaL_error(state,
                          "Source offset cannot be less than 0 for Bytedata");
      }
      if (static_cast<size_t>(sourceOffset) > buffer->GetSize()) {
        return luaL_error(state, "Source offset is out of bounds");
      }
    }

    auto *bytedata = LuaWrap::ObjectFromLua<Data::ByteData>(state, 2);
    if (bytedata == nullptr) {
      return luaL_error(state, "Expected ByteData or table as second argument");
    }

    const auto dataSpan = bytedata->GetDataSpan();

    data.resize(dataSpan.size());

    // NOLINTNEXTLINE; pointer arithmetic
    std::memcpy(data.data(), dataSpan.data() + sourceOffset, dataSpan.size());
  }

  VkDeviceSize offset = 0;
  if (lua_gettop(state) >= 3) {
    offset = static_cast<VkDeviceSize>(luaL_checkinteger(state, 3));
  }

  VkDeviceSize size = VK_WHOLE_SIZE;
  if (lua_gettop(state) >= 4) {
    size = static_cast<VkDeviceSize>(luaL_checkinteger(state, 4));
  }

  auto result = buffer->GetBuffer()->SetData(*ctx, data, offset, size);
  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to set buffer data: %s",
                      result.message.c_str());
  }

  return 0;
}

auto wrap_CopyTo(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current GraphicsContext set for this thread.");
  }

  auto *srcBuffer =
      LuaWrap::ObjectFromLua<Graphics::StructuredBuffer>(state, 1);
  auto *dstBuffer =
      LuaWrap::ObjectFromLua<Graphics::StructuredBuffer>(state, 2);

  if (srcBuffer == nullptr || dstBuffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first and second argument");
  }

  auto srcIndex = static_cast<size_t>(luaL_checkinteger(state, 3));
  auto dstIndex = static_cast<size_t>(luaL_checkinteger(state, 4));
  auto size = static_cast<size_t>(luaL_checkinteger(state, 5)); // NOLINT

  auto result = srcBuffer->GetBuffer()->CopyTo(*ctx, *dstBuffer->GetBuffer(),
                                               srcIndex, dstIndex, size);
  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to copy buffer data: %s",
                      result.message.c_str());
  }

  return 0;
}

auto wrap_GetComponentOffset(lua_State *state) -> int {
  auto *buffer = LuaWrap::ObjectFromLua<Graphics::StructuredBuffer>(state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  if (lua_type(state, 2) == LUA_TSTRING) {
    auto [name, keyCount] = ResourceKeyFromLua(state, 2);
    auto offsetResult = buffer->GetFormat().GetComponentOffset(name);
    if (Error::IsError(offsetResult)) {
      return luaL_error(state, "Failed to get component offset: %s",
                        offsetResult.error().message.c_str());
    }

    lua_pushinteger(state, static_cast<lua_Integer>(offsetResult.value()));
    return 1;
  }

  if (lua_type(state, 2) == LUA_TNUMBER) {
    auto index = static_cast<size_t>(luaL_checkinteger(state, 2));
    auto offset = buffer->GetFormat().GetComponentOffset(index);
    lua_pushinteger(state, static_cast<lua_Integer>(offset));
    return 1;
  }

  return luaL_error(state, "Expected string or integer as second argument");
}

auto wrap_GetDebugName(lua_State *state) -> int {
  auto *buffer = LuaWrap::ObjectFromLua<Graphics::StructuredBuffer>(state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  lua_pushstring(state, buffer->GetBuffer()->debugName.c_str());
  return 1;
}

// Buffer format: { { name = ..., format = ... }, ... }
inline auto BufferFormatFromLua(lua_State *state, int index)
    -> Result<Graphics::BufferFormat> {
  std::vector<Graphics::BufferComponent> components;

  if (lua_type(state, index) == LUA_TSTRING) {
    // Single component format
    return Wrap::Graphics::SimpleFormatFromLua(state, index);
  }

  return Wrap::Graphics::FormatFromLua(state, index);
}

inline auto ConditionalFlag(lua_State *state, VkBufferUsageFlags flag) {
  if (lua_toboolean(state, -1) != 0) {
    lua_pop(state, 1); // pop boolean
    return flag;
  }
  lua_pop(state, 1); // pop boolean

  return static_cast<VkBufferUsageFlags>(0);
}

// usages: "shaderstorage", "uniform", "vertex", "index"
//
// default usages: shaderstorage = false, uniform = false, vertex = false, index = false
//
// format: { { name = ..., format = ... } }
// element count: integer
// settings: { usages, cpupersistent: boolean }
auto wrap_NewBuffer(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current GraphicsContext set for this thread.");
  }

  auto formatResult = BufferFormatFromLua(state, 1);

  if (Error::IsError(formatResult)) {
    return luaL_error(state, "%s", formatResult.error().message.c_str());
  }

  auto format = formatResult.value();

  auto elementCount = static_cast<size_t>(luaL_checkinteger(state, 2));

  VkMemoryPropertyFlags memoryFlags = 0;
  VkBufferUsageFlags usageFlags = 0;

  if (lua_gettop(state) >= 3) {
    luaL_checktype(state, 3, LUA_TTABLE);

    lua_getfield(state, 3, "shaderstorage");
    usageFlags |= ConditionalFlag(state, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    lua_getfield(state, 3, "uniform");
    usageFlags |= ConditionalFlag(state, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    lua_getfield(state, 3, "vertex");
    usageFlags |= ConditionalFlag(state, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    lua_getfield(state, 3, "index");
    usageFlags |= ConditionalFlag(state, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    lua_getfield(state, 3, "cpupersistent");
    memoryFlags |=
        ConditionalFlag(state, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    lua_getfield(state, 3, "gpuonly");
    if (lua_toboolean(state, -1) != 0) {
      lua_pop(state, 1); // pop boolean

      if ((memoryFlags &
           (static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) != 0U) {
        return luaL_error(state,
                          "Buffer cannot be both cpu persistent and gpu only.");
      }

      memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    } else {
      lua_pop(state, 1); // pop boolean
    }
  }

  if (usageFlags == 0) {
    usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }

  if ((memoryFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0U) {
    usageFlags |=
        VK_BUFFER_USAGE_TRANSFER_DST_BIT; // allow data upload if not gpu only
  }

  Graphics::StructuredBufferCreationInfo info;
  info.memoryFlags = memoryFlags;
  info.usageFlags = usageFlags;
  info.debugName = "Structured Buffer";

  auto result =
      Graphics::StructuredBuffer::Create(*ctx, format, elementCount, info);

  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to create Buffer: %s",
                      result.error().message.c_str());
  }

  auto buffer = result.value();

  LuaWrap::PushObject(state, Graphics::StructuredBuffer::GetType(),
                      buffer.get());

  return 1;
}
} // namespace Graphics