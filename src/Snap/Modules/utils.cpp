#include "utils.hpp"
#include "Modules/console.hpp"
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

} // namespace Utils