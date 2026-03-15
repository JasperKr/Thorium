#pragma once

#include <cstddef>
#include <cstdint>
namespace Hash {

struct Hasher {
  size_t value = 0;

  Hasher() = default;

  auto add(size_t value) -> Hasher & {
    constexpr uint32_t prime = 0x9e3779b9;
    constexpr uint32_t shift = 6;
    constexpr uint32_t shift2 = 2;

    value ^= value + prime + (value << shift) + (value >> shift2);
    return *this;
  }

  [[nodiscard]] auto get() const -> size_t { return value; }
};

} // namespace Hash