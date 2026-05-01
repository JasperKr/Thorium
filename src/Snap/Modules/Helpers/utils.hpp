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

auto SetBindingToSlot(uint32_t set, uint32_t binding) -> uint64_t;

auto SlotToSetBinding(uint64_t slot) -> std::pair<uint32_t, uint32_t>;

template <class T, class F> constexpr void ForEachBit(T mask, F &&func) {
  static_assert(std::is_unsigned_v<T>, "mask must be unsigned");
  while (mask) {
    unsigned bit = std::countr_zero(mask);
    std::forward<F>(func)(bit);
    mask &= (mask - 1);
  }
}

// Returns the index of each set bit in the mask
// 23 -> 0, 1, 2, 4
template <class T> struct BitIndexRange {
  T mask;
  struct it {
    T m;
    auto operator*() const -> unsigned { return std::countr_zero(m); }
    auto operator++() -> it & {
      m &= (m - 1);
      return *this;
    }
    auto operator!=(const it &obj) const -> bool { return m != obj.m; }
  };
  auto begin() const -> it { return {mask}; }
  auto end() const -> it { return {0}; }
};

// Returns each set bit in the mask as a value with only that bit set
// 23 -> 1, 2, 4, 16
template <class T> struct BitMaskRange {
  T mask;
  struct it {
    T m;
    auto operator*() const -> T { return T(1) << std::countr_zero(m); }
    auto operator++() -> it & {
      m &= (m - 1);
      return *this;
    }
    auto operator!=(const it &obj) const -> bool { return m != obj.m; }
  };
  auto begin() const -> it { return {mask}; }
  auto end() const -> it { return {0}; }
};

auto GetMemoryUsage() -> size_t;

} // namespace Utils