#pragma once

#include "mathTypes.hpp"

namespace Math {

struct EulerAngle {
  // Heading, Y-axis / X-Z plane rotation
  Scalar yaw{};

  // Attitude, X-axis / Y-Z plane rotation
  Scalar pitch{};

  // Bank, Z-axis / X-Y plane rotation
  Scalar roll{};

  // NOLINTNEXTLINE, easily swappable.
  EulerAngle(Scalar yaw_val, Scalar pitch_val, Scalar roll_val)
      : yaw(yaw_val), pitch(pitch_val), roll(roll_val) {}

  EulerAngle() = default;
};

}; // namespace Math