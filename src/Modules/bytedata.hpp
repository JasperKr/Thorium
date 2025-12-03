#pragma once

#include "Modules/object.hpp"
#include <cstddef>
#include <cstdint>
namespace Data {

static const Type type = Type("ByteData");

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
} // namespace Data