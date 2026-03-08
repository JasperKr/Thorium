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

struct MemberInfo {
  std::string name;
  std::string description;
  Type const *type; // For Ref<T>
  BindingLuaType luaType;
  bool isEnum = false;
  bool isVector = false;
  const LuaWrap::LuaEnumBase *enumHelper;
};

// NOLINTNEXTLINE
inline std::vector<std::pair<std::string, std::vector<MemberInfo>>> LuaModules;

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
    return nullptr; // handled separately
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

inline auto GetLuaFieldType(const MemberInfo &member)
    -> std::vector<std::string> {
  if (member.isEnum) {
    return {std::string("snap.") + member.enumHelper->name};
  }

  if (member.luaType == BindingLuaType::Userdata && (member.type != nullptr)) {
    return {std::string("snap.") + member.type->GetName()};
  }

  const char *base = LuaTypeName(member.luaType);
  if (base == nullptr) {
    return {"any"};
  }

  if (member.isVector) {
    return {std::string(base) + "[]"};
  }

  if (member.luaType == BindingLuaType::Vec2) {
    return {"number", "number"};
  }
  if (member.luaType == BindingLuaType::Vec3) {
    return {"number", "number", "number"};
  }
  if (member.luaType == BindingLuaType::Vec4 ||
      member.luaType == BindingLuaType::Quaternion) {
    return {"number", "number", "number", "number"};
  }
  if (member.luaType == BindingLuaType::Matrix4x4) {
    return {"number[16]"};
  }

  return {base};
}

inline auto LowercaseFirstChar(const std::string &str) -> std::string {
  if (str.empty()) {
    return str;
  }
  std::string result = str;
  result[0] = static_cast<char>(std::tolower(result[0]));
  return result;
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
                          const std::vector<MemberInfo> &members) {

  out << "---@meta\n";
  out << "-- This file is auto-generated. Do not edit directly.\n";
  out << "-- See reflectBindings.hpp or the lua_stub_gen target.\n\n";
  out << "error(\"Do not require this file.\")\n\n";

  out << "---@class snap." << classname << "\n";
  out << classname << " = {}\n\n";

  for (const auto &member : members) {
    out << "--- Sets the " << member.name << " field.\n";

    if (!member.description.empty()) {
      const auto &replaced = Replace(member.description, '\n', "\n--- ");
      out << "--- " << replaced << "\n";
    }

    const auto &fields = GetLuaFieldType(member);
    auto fieldCount = fields.size();
    std::string camelCaseName = LowercaseFirstChar(member.name);

    size_t count = 0;
    for (const auto &type : fields) {
      out << "---@param ";

      if (fieldCount > 1) {
        out << camelCaseName << "_" << (++count);
      } else {
        out << camelCaseName;
      }

      out << " " << type << "\n";
    }

    out << "function " << classname << ":set" << member.name << "(";

    for (size_t index = 0; index < fieldCount; ++index) {
      if (index > 0) {
        out << ", ";
      }

      if (fieldCount > 1) {
        out << camelCaseName << "_" << (index + 1);
      } else {
        out << camelCaseName;
      }
    }

    out << ") end\n\n";
    out << "--- Gets the " << member.name << " field.\n";

    if (!member.description.empty()) {
      const auto &replaced = Replace(member.description, '\n', "\n--- ");
      out << "--- " << replaced << "\n";
    }

    out << "---@return ";
    auto index = 0;
    for (const auto &type : fields) {
      if (index > 0) {
        out << ", ";
      }
      out << type;
      index++;
    }

    out << "\nfunction " << classname << ":get" << member.name << "() end\n\n";
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