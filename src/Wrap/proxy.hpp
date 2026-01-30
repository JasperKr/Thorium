#pragma once

#include "Modules/console.hpp"
#include "Modules/object.hpp"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace LuaWrap {

inline auto ProxyFromLua(lua_State *state, int index) -> Proxy * {
  // Check if userdata
  if (lua_isuserdata(state, index) == 0) {
    return nullptr;
  }

  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));

  if (proxy == nullptr) {
    PrintWarning("ProxyFromLua: proxy is null at index {}", index);
    return nullptr;
  }

  if (proxy->type == nullptr) {
    PrintWarning("ProxyFromLua: proxy invalid type at index {}", index);
    return nullptr;
  }

  if (proxy->type->GetName() == "MODULE") {
    // Modules do not have objects
    return proxy;
  }

  if (proxy->object == nullptr) {
    PrintWarning("ProxyFromLua: proxy invalid object at index {}", index);
    return nullptr;
  }

  return proxy;
}

} // namespace LuaWrap