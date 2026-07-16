#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <utility>

namespace Math {

template <typename T, size_t N> struct StackVector {
  constexpr StackVector() = default;
  constexpr ~StackVector() = default;

  StackVector(StackVector &&) = default;
  StackVector(const StackVector &) = default;

  auto operator=(StackVector &&) -> StackVector & = default;
  auto operator=(const StackVector &) -> StackVector & = default;

  constexpr void push_back(const T &value) {
    assert(currentSize < N && "StackVector capacity exceeded");
    storage[currentSize++] = value;
  }

  constexpr void push_back(T &&value) {
    assert(currentSize < N && "StackVector capacity exceeded");
    storage[currentSize++] = std::move(value);
  }

  template <typename... Args>
  constexpr auto emplace_back(Args &&...args) -> T & {
    assert(currentSize < N && "StackVector capacity exceeded");
    storage[currentSize] = T(std::forward<Args>(args)...);
    return storage[currentSize++];
  }

  [[nodiscard]] constexpr auto capacity() const -> size_t { return N; }
  [[nodiscard]] constexpr auto max_size() const -> size_t { return N; }
  [[nodiscard]] constexpr auto size() const -> size_t { return currentSize; }

  [[nodiscard]] constexpr auto operator[](size_t index) -> T & {
    assert(index < currentSize && "Index out of bounds");
    return storage[index];
  }

  [[nodiscard]] constexpr auto operator[](size_t index) const -> const T & {
    assert(index < currentSize && "Index out of bounds");
    return storage[index];
  }

  [[nodiscard]] constexpr auto at(size_t index) -> T & {
    assert(index < currentSize && "Index out of bounds");
    return storage[index];
  }

  [[nodiscard]] constexpr auto at(size_t index) const -> const T & {
    assert(index < currentSize && "Index out of bounds");
    return storage[index];
  }

  [[nodiscard]] constexpr auto begin() -> T * { return storage.data(); }
  [[nodiscard]] constexpr auto end() -> T * {
    return storage.data() + currentSize;
  }

  [[nodiscard]] constexpr auto begin() const -> const T * {
    return storage.data();
  }
  [[nodiscard]] constexpr auto end() const -> const T * {
    return storage.data() + currentSize;
  }

  [[nodiscard]] constexpr auto data() -> T * { return storage.data(); }
  [[nodiscard]] constexpr auto data() const -> const T * {
    return storage.data();
  }

  [[nodiscard]] constexpr auto front() -> T & {
    assert(currentSize > 0 && "StackVector is empty");
    return storage[0];
  }

  [[nodiscard]] constexpr auto front() const -> const T & {
    assert(currentSize > 0 && "StackVector is empty");
    return storage[0];
  }

  [[nodiscard]] constexpr auto back() -> T & {
    assert(currentSize > 0 && "StackVector is empty");
    return storage[currentSize - 1];
  }

  [[nodiscard]] constexpr auto back() const -> const T & {
    assert(currentSize > 0 && "StackVector is empty");
    return storage[currentSize - 1];
  }

  [[nodiscard]] constexpr auto empty() const -> bool {
    return currentSize == 0;
  }
  [[nodiscard]] constexpr auto full() const -> bool { return currentSize == N; }
  constexpr auto clear() -> void {
    for (size_t i = 0; i < currentSize; ++i) {
      storage[i] = T{};
    }
    currentSize = 0;
  }

  constexpr auto pop_back() -> void {
    assert(currentSize > 0 && "Cannot pop from an empty StackVector");
    --currentSize;
    storage[currentSize] = T{};
  }

  constexpr auto operator==(const StackVector &other) const -> bool {
    if (currentSize != other.currentSize) {
      return false;
    }

    for (size_t i = 0; i < currentSize; ++i) {
      if (!(storage[i] == other.storage[i])) {
        return false;
      }
    }

    return true;
  }

  constexpr auto operator!=(const StackVector &other) const -> bool {
    return !(*this == other);
  }

  constexpr auto fill(const T &value) -> void {
    for (size_t i = 0; i < N; ++i) {
      storage[i] = value;
    }
    currentSize = N;
  }

  constexpr auto insert(size_t index, const T &value) -> void {
    assert(currentSize < N && "StackVector capacity exceeded");
    assert(index <= currentSize && "Index out of bounds");

    for (size_t i = currentSize; i > index; --i) {
      storage[i] = std::move(storage[i - 1]);
    }

    storage[index] = value;
    ++currentSize;
  }

  constexpr auto insert(size_t index, T &&value) -> void {
    assert(currentSize < N && "StackVector capacity exceeded");
    assert(index <= currentSize && "Index out of bounds");

    for (size_t i = currentSize; i > index; --i) {
      storage[i] = std::move(storage[i - 1]);
    }

    storage[index] = std::move(value);
    ++currentSize;
  }

  constexpr auto insert(T *thisStart, T *otherBegin, T *otherEnd) -> void {
    auto otherSize = static_cast<size_t>(otherEnd - otherBegin);
    assert(currentSize + otherSize <= N && "StackVector capacity exceeded");

    for (size_t i = 0; i < otherSize; ++i) {
      // NOLINTNEXTLINE
      storage[currentSize + i] = std::move(otherBegin[i]);
    }

    currentSize += otherSize;
  }

  constexpr auto erase(size_t index) -> void {
    assert(index < currentSize && "Index out of bounds");

    for (size_t i = index; i < currentSize - 1; ++i) {
      storage[i] = std::move(storage[i + 1]);
    }

    --currentSize;
    storage[currentSize] = T{};
  }

  constexpr auto resize(size_t newSize) -> void {
    assert(newSize <= N && "New size exceeds StackVector capacity");

    if (newSize < currentSize) {
      for (size_t i = newSize; i < currentSize; ++i) {
        storage[i] = T{};
      }
    }

    currentSize = newSize;
  }

  constexpr auto resize(size_t newSize, const T &value) -> void {
    assert(newSize <= N && "New size exceeds StackVector capacity");

    if (newSize > currentSize) {
      for (size_t i = currentSize; i < newSize; ++i) {
        storage[i] = value;
      }
    } else if (newSize < currentSize) {
      for (size_t i = newSize; i < currentSize; ++i) {
        storage[i] = T{};
      }
    }

    currentSize = newSize;
  }

private:
  size_t currentSize{};
  std::array<T, N> storage{};
};

} // namespace Math