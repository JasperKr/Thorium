#include "wrap_bytedata.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/color.hpp"
#include "Modules/imagedata.hpp"
#include "Wrap/wrap.hpp"
#include <cstdint>
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Wrap::Data {

auto GetBytedataFromLua(lua_State *state, int index) -> ::Data::ByteData * {
  if (LuaWrap::IsType<::Data::ByteData>(state, index)) {
    return LuaWrap::ObjectFromLua<::Data::ByteData>(state, index);
  }

  if (LuaWrap::IsType<Image::ImageData>(state, index)) {
    auto *imagedata = LuaWrap::ObjectFromLua<Image::ImageData>(state, index);
    return static_cast<::Data::ByteData *>(imagedata);
  }

  return nullptr;
}

// TODO: Change to byte offset, not element offset, otherwise unaligned writes are impossible
template <typename T>
constexpr auto SetIntFromLuaData(lua_State *state) -> int {
  auto *bytedata = GetBytedataFromLua(state, 1);
  if (bytedata == nullptr) {
    luaL_error(state, "Invalid Bytedata object.");
  }

  auto span = bytedata->GetDataSpan<uint8_t>();
  auto offset = 0L;

  if (lua_isnumber(state, 2) != 0) {
    offset = luaL_checkinteger(state, 2);
  }

  if (lua_istable(state, 3)) {
    size_t len = lua_objlen(state, 3);

    if ((len * sizeof(T)) + offset > bytedata->GetSize()) {
      return luaL_error(state, "Offset out of bounds.");
    }

    for (int i = 0; i < len; ++i) {
      lua_rawgeti(state, 3, i + 1);
      auto value = static_cast<T>(luaL_checkinteger(state, -1));
      std::memcpy(&span[offset + (i * sizeof(T))], &value, sizeof(T));
      lua_pop(state, 1);
    }
  } else if (lua_isnumber(state, 3) != 0) {
    if (offset + sizeof(T) > bytedata->GetSize()) {
      return luaL_error(state, "Offset out of bounds.");
    }

    auto value = static_cast<T>(luaL_checkinteger(state, 3));
    std::memcpy(&span[offset], &value, sizeof(T));
  }

  return 0;
}

template <typename T>
constexpr auto SetFloatFromLuaData(lua_State *state) -> int {
  auto *bytedata = GetBytedataFromLua(state, 1);
  if (bytedata == nullptr) {
    luaL_error(state, "Invalid Bytedata object.");
  }

  auto span = bytedata->GetDataSpan<uint8_t>();
  auto offset = 0L;

  if (lua_isnumber(state, 2) != 0) {
    offset = luaL_checkinteger(state, 2);
  }

  if (lua_istable(state, 3)) {
    size_t len = lua_objlen(state, 3);

    if ((len * sizeof(T)) + offset > bytedata->GetSize()) {
      return luaL_error(state, "Offset out of bounds.");
    }

    for (int i = 0; i < len; ++i) {
      lua_rawgeti(state, 3, i + 1);
      auto value = static_cast<T>(luaL_checknumber(state, -1));
      std::memcpy(&span[offset + (i * sizeof(T))], &value, sizeof(T));
      lua_pop(state, 1);
    }
  } else if (lua_isnumber(state, 3) != 0) {
    if (offset + sizeof(T) > bytedata->GetSize()) {
      return luaL_error(state, "Offset out of bounds.");
    }

    auto value = static_cast<T>(luaL_checknumber(state, 3));
    std::memcpy(&span[offset], &value, sizeof(T));
  } else {
    return luaL_error(state, "Invalid value to set.");
  }

  return 0;
}

template <typename T> constexpr auto GetIntToLuaData(lua_State *state) -> int {
  auto *bytedata = GetBytedataFromLua(state, 1);
  if (bytedata == nullptr) {
    luaL_error(state, "Invalid Bytedata object.");
  }

  auto span = bytedata->GetDataSpan<uint8_t>();
  auto offset = 0L;
  auto count = 1L;

  if (lua_isnumber(state, 2) != 0) {
    offset = luaL_checkinteger(state, 2);
  }
  if (lua_isnumber(state, 3) != 0) {
    count = luaL_checkinteger(state, 3);
  }
#ifndef NDEBUG
  if (count < 1) {
    return luaL_error(state, "Count must be at least 1.");
  }
  if ((offset + (count * sizeof(T))) > bytedata->GetSize()) {
    return luaL_error(state, "Offset out of bounds.");
  }
  if (offset % sizeof(T) != 0) {
    return luaL_error(state, "Offset must be aligned to data type size.");
  }
#endif

  for (int i = 0; i < count; ++i) {
    T output{};
    std::memcpy(&output, &span[offset + (i * sizeof(T))], sizeof(T));
    lua_pushinteger(state, output);
  }

  return static_cast<int>(count);
}

template <typename T>
constexpr auto GetFloatToLuaData(lua_State *state) -> int {
  auto *bytedata = GetBytedataFromLua(state, 1);
  if (bytedata == nullptr) {
    luaL_error(state, "Invalid Bytedata object.");
  }

  auto span = bytedata->GetDataSpan<uint8_t>();
  auto offset = 0L;
  auto count = 1L;

  if (lua_isnumber(state, 2) != 0) {
    offset = luaL_checkinteger(state, 2);
  }
  if (lua_isnumber(state, 3) != 0) {
    count = luaL_checkinteger(state, 3);
  }
#ifndef NDEBUG
  if (count < 1) {
    return luaL_error(state, "Count must be at least 1.");
  }
  if ((offset + (count * sizeof(T))) > bytedata->GetSize()) {
    return luaL_error(state, "Offset out of bounds.");
  }
  if (offset % sizeof(T) != 0) {
    return luaL_error(state, "Offset must be aligned to data type size.");
  }
#endif
  for (int i = 0; i < count; ++i) {
    T value{};
    std::memcpy(&value, &span[offset + (i * sizeof(T))], sizeof(T));
    lua_pushnumber(state, value);
  }

  return static_cast<int>(count);
}

// value, offset
// or table, offset
auto wrap_SetUInt32(lua_State *state) -> int {
  return SetIntFromLuaData<uint32_t>(state);
}
auto wrap_SetInt32(lua_State *state) -> int {
  return SetIntFromLuaData<int32_t>(state);
}
auto wrap_SetUInt16(lua_State *state) -> int {
  return SetIntFromLuaData<uint16_t>(state);
}
auto wrap_SetInt16(lua_State *state) -> int {
  return SetIntFromLuaData<int16_t>(state);
}
auto wrap_SetUInt8(lua_State *state) -> int {
  return SetIntFromLuaData<uint8_t>(state);
}
auto wrap_SetInt8(lua_State *state) -> int {
  return SetIntFromLuaData<int8_t>(state);
}

auto wrap_SetFloat(lua_State *state) -> int {
  return SetFloatFromLuaData<float>(state);
}
auto wrap_SetHalf(lua_State *state) -> int {
  return SetFloatFromLuaData<numeric::float16_t>(state);
}

auto wrap_GetUInt32(lua_State *state) -> int {
  return GetIntToLuaData<uint32_t>(state);
}
auto wrap_GetInt32(lua_State *state) -> int {
  return GetIntToLuaData<int32_t>(state);
}
auto wrap_GetUInt16(lua_State *state) -> int {
  return GetIntToLuaData<uint16_t>(state);
}
auto wrap_GetInt16(lua_State *state) -> int {
  return GetIntToLuaData<int16_t>(state);
}
auto wrap_GetUInt8(lua_State *state) -> int {
  return GetIntToLuaData<uint8_t>(state);
}
auto wrap_GetInt8(lua_State *state) -> int {
  return GetIntToLuaData<int8_t>(state);
}

auto wrap_GetFloat(lua_State *state) -> int {
  return GetFloatToLuaData<float>(state);
}
auto wrap_GetHalf(lua_State *state) -> int {
  return GetFloatToLuaData<numeric::float16_t>(state);
}

auto wrap_GetSize(lua_State *state) -> int {
  auto *bytedata = GetBytedataFromLua(state, 1);
  if (bytedata == nullptr) {
    luaL_error(state, "Invalid Bytedata object.");
  }

  lua_pushinteger(state, static_cast<lua_Integer>(bytedata->GetSize()));
  return 1;
}
auto wrap_GetPointer(lua_State *state) -> int {
  auto *bytedata = GetBytedataFromLua(state, 1);
  if (bytedata == nullptr) {
    luaL_error(state, "Invalid Bytedata object.");
  }

  lua_pushlightuserdata(state, static_cast<void *>(bytedata->GetData()));
  return 1;
}

auto wrap_NewBytedata(lua_State *state) -> int {
  auto size = static_cast<size_t>(luaL_checkinteger(state, 1));
  // auto *bytedata = new ::Data::ByteData(size); // NOLINT
  auto bytedata = Ref<::Data::ByteData>::Make(size);

  LuaWrap::PushObject(state, ::Data::ByteData::GetType(), bytedata.get());
  // bytedata->release(); // Release C++ reference, Lua now owns it

  return 1;
}

} // namespace Wrap::Data