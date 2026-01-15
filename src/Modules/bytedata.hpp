#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
namespace Data {

static const Type type = Type("ByteData");

struct ByteData : Object {
public:
  static auto GetType() -> Type const * { return &type; }

  explicit ByteData(size_t size) : size(size), data(new uint8_t[size]) {}

  ByteData(const uint8_t *src, size_t size)
      : size(size), data(new uint8_t[size]) {
    std::memcpy(data, src, size);
  }

  ByteData(const std::byte *src, size_t size)
      : size(size), data(new uint8_t[size]) {
    std::memcpy(data, src, size);
  }

  // deep copy
  ByteData(const ByteData &other)
      : size(other.size), data(new uint8_t[other.size]) {
    std::memcpy(data, other.data, size);
  }

  // move
  ByteData(ByteData &&other) noexcept : size(other.size), data(other.data) {
    other.data = nullptr;
    other.size = 0;
  }

  // deep-copy assignment
  auto operator=(const ByteData &other) -> ByteData & {
    if (this != &other) {
      delete[] data; // free old
      size = other.size;
      data = new uint8_t[size]; // allocate new data; NOLINT
      std::memcpy(data, other.data, size);
    }
    return *this;
  }

  // move assignment
  auto operator=(ByteData &&other) noexcept -> ByteData & {
    if (this != &other) {
      delete[] data;
      size = other.size;
      data = other.data;

      other.data = nullptr;
      other.size = 0;
    }
    return *this;
  }

  ~ByteData() override {
    if (parent != nullptr) {
      parent->release(); // Parent owns the data
    } else {
      delete[] data; // We own the data
    }
  }

  [[nodiscard]] auto GetSize() const -> size_t { return size; }
  auto GetData() -> uint8_t * { return data; }
  [[nodiscard]] auto GetData() const -> const uint8_t * { return data; }
  [[nodiscard]] auto GetDataSpan() const -> std::span<uint8_t> {
    return {data, size};
  }

  template <typename T> constexpr auto GetDataSpan() -> std::span<T> {
    // NOLINTNEXTLINE
    return {reinterpret_cast<T *>(data), size / sizeof(T)};
  }

  [[nodiscard]] auto Clone() const -> ByteData { return *this; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return ByteData::GetType();
  }

  [[nodiscard]] auto GetParent() const -> ByteData * { return parent; }

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  [[nodiscard]] auto View(size_t offset, size_t range)
      -> Result<Ref<ByteData>> {
    if (offset + range > size) {
      return Error::Unexpected("ByteData view out of bounds.");
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto newBytedata = ByteData(data + offset, range, this);

    this->retain(); // Retained by the view
    return Ref<ByteData>::Make(newBytedata);
  }

private:
  size_t size = 0;
  uint8_t *data = nullptr;

  ByteData *parent = nullptr;

  // private constructor for views
  ByteData(uint8_t *data, size_t size, ByteData *parent)
      : size(size), data(data), parent(parent) {}
};

} // namespace Data