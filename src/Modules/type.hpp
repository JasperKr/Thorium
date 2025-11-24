#pragma once

#include <string>
#include <utility>

class Type {
  explicit Type(const std::string &name);
  Type(std::string name, const Type &parent)
      : Name(std::move(name)), Parent(&parent) {}

public:
  auto GetName() const -> const std::string & { return Name; }
  auto GetParent() const -> const Type * { return Parent; }
  auto GetID() const -> uint32_t { return ID; }

private:
  std::string Name;
  const Type *Parent{nullptr};
  uint32_t ID{};
};