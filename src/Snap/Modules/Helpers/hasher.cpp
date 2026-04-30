#include "hasher.hpp"
#include <cstdint>
#include <functional>

namespace Hash {

auto Hasher::Add(size_t value) -> Hasher & {
  constexpr uint32_t prime = 0x9e3779b9;
  constexpr uint32_t shift = 6;
  constexpr uint32_t shift2 = 2;

  hashedValue ^= value + prime + (value << shift) + (value >> shift2);
  return *this;
}

auto Hasher::Add(void *ptr) -> Hasher & {
  return Add(std::hash<void *>()(ptr));
}

auto Hasher::Get() const -> size_t { return hashedValue; }

} // namespace Hash