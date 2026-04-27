#include "utils.hpp"
#include "Modules/console.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace Utils {

auto InterleaveSpans(
    std::vector<std::pair<std::span<const uint8_t>, size_t>> &spans)
    -> std::vector<uint8_t> {
  assert(!spans.empty());
  size_t count = spans.size();

  size_t elementsPerSpan = 0;

  // compute total output size
  size_t totalSize = 0;
  for (const auto &[span, elemSize] : spans) {
    totalSize += span.size();
    if (elementsPerSpan == 0) {
      elementsPerSpan = span.size() / elemSize;
    } else {
      assert(elementsPerSpan == (span.size() / elemSize));
    }
  }

  std::vector<uint8_t> output(totalSize);

  size_t offset = 0;
  for (size_t i = 0; i < elementsPerSpan; ++i) {
    for (const auto &[span, size] : spans) {
      std::memcpy(&output[offset], span.data() + (i * size), size); // NOLINT
      offset += size;
    }
  }

  return output;
}

auto AlignUp(size_t value, size_t alignment) -> size_t {
  assert(alignment != 0 && (alignment & (alignment - 1)) == 0 &&
         "Alignment must be a non-zero power of two.");
  return (value + alignment - 1) & ~(alignment - 1);
}

auto AlignDown(size_t value, size_t alignment) -> size_t {
  assert(alignment != 0 && (alignment & (alignment - 1)) == 0 &&
         "Alignment must be a non-zero power of two.");
  return value & ~(alignment - 1);
}

auto Subspan(std::span<const uint8_t> span, size_t offset, size_t size)
    -> std::span<const uint8_t> {
  auto range = std::min(offset + size, span.size());
  return span.subspan(offset, range - offset);
}

auto Subspan(std::vector<uint8_t> &vec, size_t offset, size_t size)
    -> std::span<uint8_t> {
  auto range = std::min(offset + size, vec.size());
  // NOLINTNEXTLINE, because of pointer arithmetic
  return {vec.data() + offset, range - offset};
}

} // namespace Utils