#pragma once

#include <cstddef>
namespace Hash {

struct Hasher {
  Hasher() = default;

  auto Add(size_t value) -> Hasher &;
  auto Add(void *ptr) -> Hasher &;

  [[nodiscard]] auto Get() const -> size_t;

private:
  size_t hashedValue = 0;
};

} // namespace Hash