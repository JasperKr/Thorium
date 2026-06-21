#pragma once

#include "mathTypes.hpp"
#include <numbers>
#include <span>
#include <string>

namespace Math {

struct EulerAngle {
  // Heading, Y-axis / X-Z plane rotation
  Scalar yaw{};

  // Attitude, X-axis / Y-Z plane rotation
  Scalar pitch{};

  // Bank, Z-axis / X-Y plane rotation
  Scalar roll{};

  // NOLINTNEXTLINE, easily swappable.
  constexpr EulerAngle(Scalar yaw_val, Scalar pitch_val, Scalar roll_val)
      : yaw(yaw_val), pitch(pitch_val), roll(roll_val) {}

  constexpr EulerAngle() = default;

  [[nodiscard]] auto ToString() const -> std::string {
    return "(Yaw: " + std::to_string(yaw) +
           ", Pitch: " + std::to_string(pitch) +
           ", Roll: " + std::to_string(roll) + ")";
  }

  [[nodiscard]] auto Span() const -> std::span<const Scalar, 3> {
    return std::span<const Scalar, 3>(&yaw, 3);
  }
  [[nodiscard]] auto Ptr() const -> const Scalar * { return &yaw; }
  [[nodiscard]] auto Ptr() -> Scalar * { return &yaw; }

  static constexpr auto DegToRad(Scalar degrees) -> Scalar {
    return degrees * static_cast<Scalar>(std::numbers::pi) /
           static_cast<Scalar>(180.0F); // NOLINT
  }

  static constexpr auto RadToDeg(Scalar radians) -> Scalar {
    return radians * static_cast<Scalar>(180.0F) / // NOLINT
           static_cast<Scalar>(std::numbers::pi);
  }

  [[nodiscard]] constexpr auto ToRadians() const -> EulerAngle {
    return {DegToRad(yaw), DegToRad(pitch), DegToRad(roll)};
  }

  [[nodiscard]] constexpr auto ToDegrees() const -> EulerAngle {
    return {RadToDeg(yaw), RadToDeg(pitch), RadToDeg(roll)};
  }
};

}; // namespace Math