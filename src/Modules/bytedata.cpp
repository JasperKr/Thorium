/*
struct ByteData : Object {
public:
  static auto GetType() -> Type { return type; }

  explicit ByteData(size_t size);
  ByteData(const ByteData &other);
  ByteData(const uint8_t *data, size_t size);
  ~ByteData() override;

  [[nodiscard]] auto GetSize() const -> size_t;
  auto GetData() -> uint8_t *;
  [[nodiscard]] auto GetData() const -> const uint8_t *;
  [[nodiscard]] auto Clone() const -> ByteData;

private:
  size_t size;
  uint8_t *data;
};
*/

#include "bytedata.hpp"
#include <cstring>

namespace Data {
ByteData::ByteData(size_t size) : size(size), data(new uint8_t[size]) {}

ByteData::ByteData(const ByteData &other)
    : size(other.size), data(new uint8_t[other.size]) {
  std::memcpy(data, other.data, other.size);
}

ByteData::ByteData(const uint8_t *data, size_t size)
    : size(size), data(new uint8_t[size]) {
  std::memcpy(this->data, data, size);
}

ByteData::~ByteData() { delete[] data; }
auto ByteData::GetSize() const -> size_t { return size; }
auto ByteData::GetData() -> uint8_t * { return data; }
auto ByteData::GetData() const -> const uint8_t * { return data; }
auto ByteData::Clone() const -> ByteData { return {*this}; }
} // namespace Data