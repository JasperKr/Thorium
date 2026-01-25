#include "lua_data.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Wrap/proxy.hpp"
#include "Wrap/wrap.hpp"
#include <string>
#include <variant>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace LuaWrap::Data {

auto ToStack(lua_State *state, const LuaType &data) -> Error {
  if (std::holds_alternative<bool>(data)) {
    lua_pushboolean(state, std::get<bool>(data) ? 1 : 0);
  } else if (std::holds_alternative<double>(data)) {
    lua_pushnumber(state, std::get<double>(data));
  } else if (std::holds_alternative<std::string>(data)) {
    const auto &str = std::get<std::string>(data);
    lua_pushlstring(state, str.c_str(), str.size());
  } else if (std::holds_alternative<Proxy>(data)) {
    const auto &proxy = std::get<Proxy>(data);
    PrintAlways("ToStack: Type: {}, Object ptr: {}", proxy.type->GetName(),
                (void *)proxy.object);
    LuaWrap::PushObject(state, proxy.type, proxy.object);
  } else if (std::holds_alternative<std::vector<LuaData>>(data)) {
    lua_newtable(state);
    const auto &tableData = std::get<std::vector<LuaData>>(data);
    for (const auto &entry : tableData) {
      auto keyResult = ToStack(state, entry.key);
      if (Error::IsError(keyResult)) {
        return keyResult;
      }

      auto valueResult = ToStack(state, entry.value);
      if (Error::IsError(valueResult)) {
        return valueResult;
      }

      lua_settable(state, -3);
    }
  } else if (std::holds_alternative<std::monostate>(data)) {
    lua_pushnil(state);
  } else {
    return Error::Create("Unsupported LuaType variant for ToStack.");
  }

  return Error::Success();
}

auto FromStack(lua_State *state, int index) -> Result<LuaType> {
  switch (lua_type(state, index)) {
  case LUA_TNIL:
    return Error::Unexpected("Cannot convert nil to LuaType");
  case LUA_TBOOLEAN:
    return lua_toboolean(state, index) != 0;
  case LUA_TNUMBER:
    return static_cast<double>(lua_tonumber(state, index));
  case LUA_TSTRING: {
    size_t len = 0;
    const char *data = lua_tolstring(state, index, &len);

    if (data != nullptr && len > 0) {
      return std::string(data, len);
    }

    return std::string();
  }
  case LUA_TUSERDATA: {
    // NOLINTNEXTLINE
    auto *proxy = LuaWrap::ProxyFromLua(state, index);
    if (proxy == nullptr) {
      return Error::Unexpected("Invalid userdata proxy.");
    }
    return *proxy;
  }
  case LUA_TTABLE: {
    std::vector<LuaData> tableData;

    lua_pushnil(state); // first key
    while (lua_next(state, index) != 0) {
      // key is at -2, value at -1
      auto valueResult = FromStack(state, -1);
      if (Error::IsError(valueResult)) {
        return valueResult;
      }

      auto keyResult = FromStack(state, -2);
      if (Error::IsError(keyResult)) {
        return keyResult;
      }

      tableData.emplace_back(LuaData{
          .key = keyResult.value(),
          .value = valueResult.value(),
      });

      lua_pop(state, 1); // remove value, keep key for next iteration
    }

    return tableData;
  }
  default:
    return Error::Unexpected("Unsupported Lua type for LuaType conversion");
  }
}

} // namespace LuaWrap::Data
