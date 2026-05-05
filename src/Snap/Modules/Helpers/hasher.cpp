#include "hasher.hpp"
#include <cstdint>
#include <functional>

namespace Hash {

auto Hasher::Add(size_t value) -> void {
  constexpr uint32_t prime = 0x9e3779b9;
  constexpr uint32_t shift = 6;
  constexpr uint32_t shift2 = 2;

  hashedValue ^= value + prime + (value << shift) + (value >> shift2);
}

auto Hasher::Add(void *ptr) -> void { Add(std::hash<void *>()(ptr)); }

auto Hasher::Get() const -> size_t { return hashedValue; }
auto Hasher::Reset() -> void { hashedValue = 0; }

} // namespace Hash