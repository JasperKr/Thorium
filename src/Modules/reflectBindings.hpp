#pragma once

#include "Modules/Math/matrix.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/type.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
namespace Bindings {

enum class BindingLuaType : uint8_t {
  // Default types
  Integer,
  Number,
  Boolean,
  String,
  Table,
  Userdata,

  // Special types with custom handling
  Vec2,
  Vec3,
  Vec4,
  Quaternion,
  Matrix4x4,
};

template <typename T> constexpr auto GetBindingLuaType() {
  if constexpr (std::is_integral_v<T>) {
    return BindingLuaType::Integer;
  } else if constexpr (std::is_floating_point_v<T>) {
    return BindingLuaType::Number;
  } else if constexpr (std::is_same_v<T, bool>) {
    return BindingLuaType::Boolean;
  } else if constexpr (std::is_same_v<T, std::string> ||
                       std::is_same_v<T, const char *> ||
                       std::is_same_v<T, std::basic_string<char>>) {
    return BindingLuaType::String;
  } else if constexpr (std::is_same_v<T, Math::Vec2> ||
                       std::is_same_v<T, Math::Uvec2> ||
                       std::is_same_v<T, Math::Ivec2>) {
    return BindingLuaType::Vec2;
  } else if constexpr (std::is_same_v<T, Math::Vec3> ||
                       std::is_same_v<T, Math::Uvec3> ||
                       std::is_same_v<T, Math::Ivec3>) {
    return BindingLuaType::Vec3;
  } else if constexpr (std::is_same_v<T, Math::Vec4> ||
                       std::is_same_v<T, Math::Uvec4> ||
                       std::is_same_v<T, Math::Ivec4>) {
    return BindingLuaType::Vec4;
  } else if constexpr (std::is_same_v<T, Math::Quaternion>) {
    return BindingLuaType::Quaternion;
  } else if constexpr (std::is_same_v<T, Math::Matrix4x4>) {
    return BindingLuaType::Matrix4x4;
  } else {
    return BindingLuaType::Userdata; // Default to userdata for complex types
  }
}

struct TypeInfo {
  std::string name;

  Type const *type; // For Ref<T>
  BindingLuaType luaType;
  bool isEnum = false;
  bool isVector = false;
  const LuaWrap::LuaEnumBase *enumHelper;
};

struct MethodInfo {
  std::string name;
  std::string description;

  std::vector<TypeInfo> parameters;
  std::optional<TypeInfo> returnType;
};

// NOLINTNEXTLINE
inline std::vector<std::pair<std::string, std::vector<MethodInfo>>> LuaModules;

constexpr auto LuaTypeName(BindingLuaType bindingType) -> const char * {
  switch (bindingType) {
  case BindingLuaType::Integer:
    return "integer";
  case BindingLuaType::Number:
    return "number";
  case BindingLuaType::Boolean:
    return "boolean";
  case BindingLuaType::String:
    return "string";
  case BindingLuaType::Table:
    return "table";
  case BindingLuaType::Userdata:
    return "USERDATA"; // handled separately
  case BindingLuaType::Vec2:
    return "Vec2";
  case BindingLuaType::Vec3:
    return "Vec3";
  case BindingLuaType::Vec4:
    return "Vec4";
  case BindingLuaType::Quaternion:
    return "Quaternion";
  case BindingLuaType::Matrix4x4:
    return "Matrix4x4";
  }
  return "any";
}

inline auto LowercaseFirstChar(const std::string &str) -> std::string {
  if (str.empty()) {
    return str;
  }
  std::string result = str;
  result[0] = static_cast<char>(std::tolower(result[0]));
  return result;
}

inline auto GetLuaTypes(const std::vector<TypeInfo> &methods)
    -> std::vector<std::pair<std::string, std::string>> {
  std::vector<std::pair<std::string, std::string>> types;

  for (const auto &typeinfo : methods) {
    const auto &camelCaseName = LowercaseFirstChar(typeinfo.name);

    if (typeinfo.isEnum) {
      types.emplace_back(std::string("snap.") + typeinfo.enumHelper->name,
                         camelCaseName);
      continue;
    }

    if (typeinfo.luaType == BindingLuaType::Userdata &&
        (typeinfo.type != nullptr)) {
      types.emplace_back(std::string("snap.") + typeinfo.type->GetName(),
                         camelCaseName);
      continue;
    }

    const char *base = LuaTypeName(typeinfo.luaType);
    if (base == nullptr) {
      types.emplace_back("any", camelCaseName);
    } else if (typeinfo.isVector) {
      types.emplace_back(std::string(base) + "[]", camelCaseName);
    } else if (typeinfo.luaType == BindingLuaType::Vec2) {
      types.emplace_back("number", camelCaseName + "_x");
      types.emplace_back("number", camelCaseName + "_y");
    } else if (typeinfo.luaType == BindingLuaType::Vec3) {
      types.emplace_back("number", camelCaseName + "_x");
      types.emplace_back("number", camelCaseName + "_y");
      types.emplace_back("number", camelCaseName + "_z");
    } else if (typeinfo.luaType == BindingLuaType::Vec4 ||
               typeinfo.luaType == BindingLuaType::Quaternion) {
      types.emplace_back("number", camelCaseName + "_x");
      types.emplace_back("number", camelCaseName + "_y");
      types.emplace_back("number", camelCaseName + "_z");
      types.emplace_back("number", camelCaseName + "_w");
    } else if (typeinfo.luaType == BindingLuaType::Matrix4x4) {
      types.emplace_back("number[16]", camelCaseName);
    } else {
      types.emplace_back(base, camelCaseName);
    }
  }

  return types;
}

inline auto Replace(const std::string &src, char from,
                    const std::string &toReplace) -> std::string {
  std::string result;
  size_t start = 0;
  size_t pos = 0;
  while ((pos = src.find(from, start)) != std::string::npos) {
    result += src.substr(start, pos - start) + toReplace;
    start = pos + 1;
  }
  result += src.substr(start);
  return result;
}

inline void EmitLuaStruct(std::ostream &out, const std::string &classname,
                          const std::vector<MethodInfo> &members) {

  out << "---@meta\n";
  out << "-- This file is auto-generated. Do not edit directly.\n";
  out << "-- See reflectBindings.hpp or the lua_stub_gen target.\n\n";
  out << "error(\"Do not require this file.\")\n\n";

  out << "---@class snap." << classname << " : snap.Data\n";
  out << classname << " = {}\n\n";

  for (const auto &member : members) {
    if (!member.description.empty()) {
      const auto &replaced = Replace(member.description, '\n', "\n--- ");
      out << "--- " << replaced << "\n";
    }

    const auto &parameters = GetLuaTypes(member.parameters);
    auto fieldCount = parameters.size();

    for (const auto &type : parameters) {
      out << "---@param ";

      const auto &typeName = type.first;
      const auto &paramName = type.second;

      out << (paramName.empty() ? "param" : paramName) << " "
          << (typeName.empty() ? "any" : typeName) << "\n";
    }

    out << "---@return ";
    if (member.returnType.has_value()) {
      const auto &returnTypes = GetLuaTypes({member.returnType.value()});
      for (size_t index = 0; index < returnTypes.size(); ++index) {
        if (index > 0) {
          out << "---@return ";
        }

        const auto &typeName = returnTypes[index].first;
        const auto &paramName = returnTypes[index].second;

        out << (typeName.empty() ? "any" : typeName) << " "
            << (paramName.empty() ? "Unknown" : paramName) << "\n";
      }

      if (returnTypes.empty()) {
        out << "any\n";
      }
    } else {
      out << "nil\n";
    }

    out << "function " << classname << ":" << member.name << "(";

    for (size_t index = 0; index < fieldCount; ++index) {
      if (index > 0) {
        out << ", ";
      }

      out << parameters[index].second;
    }

    out << ") end\n\n";
  }
}

inline void EmitLuaEnums(std::ostream &out,
                         const std::vector<LuaWrap::LuaEnumBase> &enums) {
  out << "---@meta\n";
  out << "-- This file is auto-generated. Do not edit directly.\n";
  out << "-- See reflectBindings.hpp or the lua_stub_gen target.\n\n";
  out << "error(\"Do not require this file.\")\n\n";

  for (const auto &luaEnum : enums) {
    out << "---@alias snap." << luaEnum.name << " ";
    bool first = true;
    for (const auto &option : luaEnum.options) {
      if (!first) {
        out << " | ";
      }

      out << "\"" << option << "\"";
      first = false;
    }
    out << "\n\n";
  }
}

} // namespace Bindings