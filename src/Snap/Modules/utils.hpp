#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>
namespace Utils {

template <typename T, typename Pred>
void UnorderedErase(std::vector<T> &vect, Pred &&predicate) {
  for (std::size_t i = 0; i < vect.size();) {
    if (std::forward<Pred>(predicate)(vect[i])) {
      vect[i] = std::move(vect.back());
      vect.pop_back();
    } else {
      ++i;
    }
  }
}

auto InterleaveSpans(
    std::vector<std::pair<std::span<const uint8_t>, size_t>> &spans)
    -> std::vector<uint8_t>;

auto AlignUp(size_t value, size_t alignment) -> size_t;
auto AlignDown(size_t value, size_t alignment) -> size_t;

auto Subspan(std::span<const uint8_t> span, size_t offset, size_t size)
    -> std::span<const uint8_t>;
auto Subspan(std::vector<uint8_t> &vec, size_t offset, size_t size)
    -> std::span<uint8_t>;

} // namespace Utils