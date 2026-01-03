#pragma once

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

} // namespace Utils