#pragma once

#include "customFloat.hpp"
#include <cstdint>

namespace Math {
using Scalar = float;

using Float11 = CustomFloat<uint16_t, 5, 6, false>; // NOLINT magic numbers
using Float10 = CustomFloat<uint16_t, 5, 5, false>; // NOLINT magic numbers

} // namespace Math