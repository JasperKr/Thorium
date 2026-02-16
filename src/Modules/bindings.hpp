// Lua bindings generator

#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Wrap/wrap.hpp"
#include <cassert>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Bindings {

template <typename> struct is_ref : std::false_type {};
template <typename T> struct is_ref<Ref<T>> : std::true_type {
  using element_type = T;
};

// Check if R is a Ref<O> Where O inherits from Object.
template <typename R>
concept RefOfObject =
    is_ref<R>::value &&
    std::is_base_of_v<Object, typename is_ref<R>::element_type>;

// Pointer or reference types are returned as pointers, value types are returned as values.
template <typename T>
using LuaGetReturn = std::conditional_t<
    std::is_arithmetic_v<std::remove_cvref_t<T>> ||
        std::is_same_v<std::remove_cvref_t<T>, std::string> || RefOfObject<T>,
    std::remove_cvref_t<T>,  // value types
    std::remove_cvref_t<T> * // userdata types
    >;

template <typename> struct MemberTraits;

template <typename T, typename M> struct MemberTraits<M T::*> {
  using Object = T;
  using Type = M;
};

template <typename T> struct LuaType {
  // Returns either a value (for arithmetic and string types) or a pointer (for userdata types)
  template <typename M>
  static auto GetValue(lua_State *state, int stackIndex)
      -> Result<LuaGetReturn<M>> {
    if constexpr (std::is_floating_point_v<M>) {
      return static_cast<M>(luaL_checknumber(state, stackIndex));
    } else if constexpr (std::is_same_v<M, bool>) {
      return lua_toboolean(state, stackIndex) != 0;
    } else if constexpr (std::is_integral_v<M>) {
      return static_cast<M>(luaL_checkinteger(state, stackIndex));
    } else if constexpr (std::is_same_v<M, std::string>) {
      size_t len = 0;
      const char *str = luaL_checklstring(state, stackIndex, &len);
      return std::string(str, len);
    } else if constexpr (LuaWrap::LuaObject<M>) {
      return LuaWrap::ResultFromLua<M>(state, stackIndex);
    } else {
      return Error::Unexpected("Unsupported type for Lua bindings");
    }

    return Error::Unexpected("Unsupported type for Lua bindings");
  };

  template <typename M>
  static auto PushValue(lua_State *state, const M &value) -> void {
    if constexpr (std::is_floating_point_v<M>) {
      lua_pushnumber(state, static_cast<lua_Number>(value));
    } else if constexpr (std::is_same_v<M, bool>) {
      lua_pushboolean(state, value);
    } else if constexpr (std::is_integral_v<M>) {
      lua_pushinteger(state, static_cast<lua_Integer>(value));
    } else if constexpr (std::is_same_v<M, std::string>) {
      lua_pushlstring(state, value.c_str(), value.size());
    } else if constexpr (LuaWrap::LuaObject<M>) {
      LuaWrap::PushObject(state, M::GetType(), const_cast<M *>(&value));
    } else if (RefOfObject<M>) {
      using O = typename M::element_type;
      LuaWrap::PushObject(state, O::GetType(), value.get());
    } else {
    }
  };

  template <auto Member> static auto GenBindings_Get() -> lua_CFunction {
    using MemberPtr = decltype(Member); // e.g., int Test::*
    using Traits = MemberTraits<MemberPtr>;

    return [](lua_State *state) -> int {
      auto obj = LuaWrap::ResultFromLua<Traits::Object>(state, 1);
      if (Error::IsError(obj)) {
        return luaL_error(state, "%s", obj.error().message.c_str());
      }

      // remove object from stack, so we don't return it to Lua if PushValue fails
      lua_pop(state, 1);

      PushValue(state, (*obj)->*Member);
      return 1;
    };
  }

  template <auto Member> static auto GenBindings_Set() -> lua_CFunction {
    using MemberPtr = decltype(Member); // e.g., int Test::*
    using Traits = MemberTraits<MemberPtr>;
    using M = typename Traits::Type;

    // God help you if it crashes here

    return [](lua_State *state) -> int {
      auto obj = LuaWrap::ResultFromLua<Traits::Object>(state, 1);
      if (Error::IsError(obj)) {
        return luaL_error(state, "%s", obj.error().message.c_str());
      }

      // Assign dereferenced pointer for userdata types, direct value for value types
      if constexpr (std::is_pointer_v<LuaGetReturn<M>>) {
        if constexpr (std::is_copy_assignable_v<M> ||
                      std::is_move_assignable_v<M>) {
          auto value = GetValue<M>(state, 2);
          if (Error::IsError(value)) {
            return luaL_error(state, "%s", value.error().message.c_str());
          }

          (*obj)->*Member = *value.value();
        } else {
          return luaL_error(state, "Type is not assignable");
        }
      } else {
        // If M is a RefOfObject, we need to assign the inner object, not the Ref itself
        if constexpr (RefOfObject<M>) {
          using O = typename M::element_type;

          auto value = LuaWrap::ResultFromLua<O>(state, 2);
          if (Error::IsError(value)) {
            return luaL_error(state, "%s", value.error().message.c_str());
          }

          (*obj)->*Member = Ref<O>(value.value());
          return 0;
        }

        auto value = GetValue<M>(state, 2);
        if (Error::IsError(value)) {
          return luaL_error(state, "%s", value.error().message.c_str());
        }

        (*obj)->*Member = value.value();
      }

      return 0;
    };
  };
};

template <typename T>
  requires LuaWrap::LuaObject<T>
struct LuaBoundStruct {
  explicit LuaBoundStruct<T>(std::string structName)
      : name(std::move(structName)) {}

  std::string name;
  std::vector<lua_CFunction> methods;
  std::vector<std::string> names;
  LuaType<T> type{};

  void AddMethod(const lua_CFunction &method) { methods.emplace_back(method); }

  template <auto Member> void RegisterMember(const std::string &name) {
    using Traits =
        MemberTraits<decltype(Member)>; // e.g., MemberTraits<int Test::*>
    using M = typename Traits::Type;    // e.g., int

    names.emplace_back(std::string("get") + name);
    AddMethod(type.template GenBindings_Get<Member>());

    names.emplace_back(std::string("set") + name);
    AddMethod(type.template GenBindings_Set<Member>());
  };

  void Register(lua_State *state) {
    assert(!name.empty() && "Struct name must be set before registering");

    std::vector<luaL_Reg> luaMethods;
    luaMethods.reserve(methods.size() + 1); // +1 for the null terminator
    for (size_t i = 0; i < methods.size(); ++i) {
      luaMethods.push_back({names[i].c_str(), methods[i]});
    }
    luaMethods.push_back({nullptr, nullptr}); // null terminator

    LuaWrap::RegisterLuaType(state, T::GetType(), luaMethods.data());
  };
};

} // namespace Bindings