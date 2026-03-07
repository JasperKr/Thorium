#pragma once

#include "Modules/type.hpp"
#include <cstdint>
#include <format>
#include <mutex>
#include <string>
#include <unordered_map>
namespace Engine {
using Identifier = uint64_t;

// Generates a globally unique identifier.
// Useful for object ID's, but not for naming since they are not human readable.
inline auto GenerateIdentifier() -> Identifier {
  static Identifier currentId = 0;
  return ++currentId;
}

// Generates a unique identifier for the given type.
// The identifier will always be unique for each type,
// But will not be globally unique. Useful for naming objects
// SHOULD NOT be used for object ID's
inline auto GenerateItentifier(Type const *type) -> Identifier {
  static std::mutex mutex;
  static std::unordered_map<std::string, Identifier> usedIds;

  std::lock_guard lock(mutex);

  const auto &typeName = type->GetName();
  auto iter = usedIds.find(typeName);
  if (iter != usedIds.end()) {
    return ++iter->second;
  }

  Identifier newId = GenerateIdentifier();
  usedIds[typeName] = newId;
  return newId;
}

// Generates a unique name for the given type, using the type name and a unique identifier.
inline auto UniqueName(const Type *type) -> std::string {
  Identifier identifier = GenerateItentifier(type);
  return std::format("{}_{}", type->GetName(), identifier);
}

} // namespace Engine