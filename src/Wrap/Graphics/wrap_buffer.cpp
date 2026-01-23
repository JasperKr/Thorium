#include "Graphics/barrier.hpp"
#include "Wrap/Graphics/wrap_reflection.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_utils.hpp"
#include <cstdint>
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
#include "wrap_buffer.hpp"

namespace Graphics::StructuredBuffer {

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
  auto *buffer =
      LuaWrap::ObjectFromLua<Graphics::StructuredBuffer::StructuredBuffer>(
          state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  lua_pushinteger(state, static_cast<lua_Integer>(buffer->GetSize()));
  return 1;
}

auto wrap_GetElementCount(lua_State *state) -> int {
  auto *buffer =
      LuaWrap::ObjectFromLua<Graphics::StructuredBuffer::StructuredBuffer>(
          state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  lua_pushinteger(state, static_cast<lua_Integer>(buffer->GetElementCount()));
  return 1;
}

auto wrap_GetElementStride(lua_State *state) -> int {
  auto *buffer =
      LuaWrap::ObjectFromLua<Graphics::StructuredBuffer::StructuredBuffer>(
          state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  lua_pushinteger(state, static_cast<lua_Integer>(buffer->GetElementStride()));
  return 1;
}

// [value = 0], [offset = 0], [size = whole size]
auto wrap_Clear(lua_State *state) -> int {
  auto *buffer =
      LuaWrap::ObjectFromLua<Graphics::StructuredBuffer::StructuredBuffer>(
          state, 1);

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

// returns:
// { { name = ..., offset = ..., format = ... } }
auto wrap_GetFormat(lua_State *state) -> int {
  auto *buffer =
      LuaWrap::ObjectFromLua<Graphics::StructuredBuffer::StructuredBuffer>(
          state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  auto format = buffer->GetFormat();

  lua_newtable(state);

  int tableIndex = 1;
  for (const auto &component : format.GetComponents()) {
    lua_newtable(state);
    // name, list of strings
    lua_pushstring(state, "name");
    lua_newtable(state);

    auto begin = component.name.begin();
    auto end = component.name.end();

    int nameIndex = 1;
    for (auto it = begin; it != end; ++it) {
      lua_pushinteger(state, nameIndex++);
      lua_pushstring(state, it->c_str());
      lua_settable(state, -3);
    }
    lua_settable(state, -3); // table["name"] = name table

    // offset
    lua_pushstring(state, "offset");
    lua_pushinteger(state, static_cast<lua_Integer>(component.offset));
    lua_settable(state, -3); // table["offset"] = offset

    // format
    lua_pushstring(state, "format");
    lua_pushstring(state,
                   Format::VertexFormatToString(component.format).c_str());
    lua_settable(state, -3); // table["format"] = format

    lua_pushinteger(state, tableIndex++);
    lua_insert(state, -2);   // move index below table
    lua_settable(state, -3); // main table[index] = component table
  }

  return 1;
}

// data: Bytedata | table of numbers, offset: integer, size: integer
auto wrap_SetData(lua_State *state) -> int {
  auto *ctx = GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current GraphicsContext set for this thread.");
  }

  auto *buffer =
      LuaWrap::ObjectFromLua<Graphics::StructuredBuffer::StructuredBuffer>(
          state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  std::vector<uint8_t> data{};

  Graphics::Barrier::UpdateUsage(*ctx, *buffer->buffer,
                                 Graphics::Barrier::ResourceState{
                                     .stages = VK_PIPELINE_STAGE_2_HOST_BIT,
                                     .access = VK_ACCESS_2_HOST_WRITE_BIT,
                                 });

  if (lua_istable(state, 2)) {
    // table of numbers
    size_t tableSize = lua_objlen(state, 2);
    data.resize(tableSize * sizeof(float));
    for (int i = 0; i < tableSize; ++i) {
      lua_rawgeti(state, 2, i + 1);
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
    auto *bytedata = LuaWrap::ObjectFromLua<Data::ByteData>(state, 2);
    if (bytedata == nullptr) {
      return luaL_error(state, "Expected ByteData or table as second argument");
    }

    const auto dataSpan = bytedata->GetDataSpan();

    data.resize(dataSpan.size());
    std::memcpy(data.data(), dataSpan.data(), dataSpan.size());
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

// Buffer format: { { name = ..., format = ... }, ... }
inline auto BufferFormatFromLua(lua_State *state, int index)
    -> Graphics::BufferFormat {
  std::vector<Graphics::BufferComponent> components;

  if (lua_type(state, index) == LUA_TSTRING) {
    // Single component format
    const char *formatStr = luaL_checkstring(state, index);
    auto vkFormat = Graphics::Format::VertexFormatStringToVkFormat(formatStr);
    if (vkFormat == VK_FORMAT_UNDEFINED) {
      luaL_error(state, "Invalid format string: %s", formatStr);
    }

    components.emplace_back(
        Graphics::BufferComponent{.name = {"default"}, .format = vkFormat});

    Graphics::BufferFormat format(components);

    if (format.GetSize() == 0) {
      luaL_error(state, "Buffer format has zero size.");
    }

    return format;
  }

  luaL_checktype(state, index, LUA_TTABLE);

  size_t tableSize = lua_objlen(state, index);

  for (int i = 0; i < tableSize; ++i) {
    lua_rawgeti(state, index, i + 1);
    lua_getfield(state, -1, "name");
    auto key = ResourceKeyFromLuaTable(state, -1);
    lua_pop(state, 1);

    lua_getfield(state, -1, "format");
    const char *formatStr = luaL_checkstring(state, -1);
    lua_pop(state, 1);

    auto vkFormat = Graphics::Format::VertexFormatStringToVkFormat(formatStr);
    if (vkFormat == VK_FORMAT_UNDEFINED) {
      luaL_error(state, "Invalid format string: %s", formatStr);
    }

    components.emplace_back(
        Graphics::BufferComponent{.name = key, .format = vkFormat});

    lua_pop(state, 1); // pop component table
  }

  Graphics::BufferFormat format(components);

  if (format.GetSize() == 0) {
    luaL_error(state, "Buffer format has zero size.");
  }

  return format;
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

  auto format = BufferFormatFromLua(state, 1);
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

  auto result = Graphics::StructuredBuffer::CreateStructuredBuffer(
      *ctx, format, elementCount, memoryFlags, usageFlags);

  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to create Buffer: %s",
                      result.error().message.c_str());
  }

  auto buffer = result.value();

  LuaWrap::PushObject(state,
                      Graphics::StructuredBuffer::StructuredBuffer::GetType(),
                      buffer.get());

  return 1;
}

auto wrap_Release(lua_State *state) -> int {
  auto *buffer =
      LuaWrap::ObjectFromLua<Graphics::StructuredBuffer::StructuredBuffer>(
          state, 1);

  if (buffer == nullptr) {
    return luaL_error(state, "Expected Buffer as first argument");
  }

  buffer->ScheduleDestroy();
  return 0;
}
} // namespace Graphics::StructuredBuffer