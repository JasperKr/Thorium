#pragma once

#include "Graphics/graphicsState.hpp"
#include "lua.hpp"

// linked list of strings as key and a count of valid entries
inline auto ResourceKeyFromLua(lua_State *state, int index)
    -> std::pair<Graphics::ResourceKey, int32_t> {
  auto count = lua_gettop(state);
  thread_local Graphics::ResourceKey root;
  root.clear();
  auto iterator = root.before_begin();

  for (int i = index; i <= count; ++i) {
    if (lua_type(state, i) == LUA_TSTRING) {
      iterator = root.insert_after(iterator, luaL_checkstring(state, i));
    } else {
      return std::make_pair(root, i - index);
    }
  }

  return std::make_pair(root, count);
}

inline auto ResourceKeyFromLuaTable(lua_State *state, int index)
    -> Graphics::ResourceKey {
  if (index < 0) {
    index = lua_gettop(state) + index + 1;
  }

  thread_local Graphics::ResourceKey root;
  root.clear();
  auto iterator = root.before_begin();

  luaL_checktype(state, index, LUA_TTABLE);
  size_t tableSize = lua_objlen(state, index);

  for (size_t i = 0; i < tableSize; ++i) {
    lua_rawgeti(state, index, static_cast<int>(i + 1));
    const char *keyPart = luaL_checkstring(state, -1);
    iterator = root.insert_after(iterator, keyPart);
    lua_pop(state, 1);
  }

  return root;
}

inline auto ResourceKeyFromSingleLuaObject(lua_State *state, int index)
    -> Graphics::ResourceKey {
  // Just a single string, treat it as a key with one part
  if (lua_type(state, index) == LUA_TSTRING) {
    return Graphics::ResourceKey{luaL_checkstring(state, index)};
  }
  // A table, treat it as a list of strings
  if (lua_type(state, index) == LUA_TTABLE) {
    return ResourceKeyFromLuaTable(state, index);
  }
  // Invalid type, return empty key
  return Graphics::ResourceKey{};
}