#pragma once

#include <cstddef>
namespace Hash {

struct Hasher {
  Hasher() = default;

  auto Add(size_t value) -> void;
  auto Add(void *ptr) -> void;

  [[nodiscard]] auto Get() const -> size_t;
  auto Reset() -> void;

private:
  size_t hashedValue = 0;
};

} // namespace Hash