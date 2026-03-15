#pragma once

#include <cstdint>
#include <string>
#include <utility>

class Type {
public:
  explicit Type(std::string name) : Name(std::move(name)) {}
  Type(std::string name, const Type &parent)
      : Name(std::move(name)), Parent(&parent) {}

  [[nodiscard]] auto GetName() const -> const std::string & { return Name; }
  [[nodiscard]] auto GetParent() const -> const Type * { return Parent; }
  [[nodiscard]] auto GetID() const -> uint32_t { return ID; }

  auto operator==(const Type &other) const -> bool {
    return Name == other.Name;
  }

private:
  std::string Name;
  const Type *Parent{nullptr};
  uint32_t ID{};
};