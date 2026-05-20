#pragma once

// #define LOG_ERRORS 1

#include "slang/slang-com-ptr.h"
#include "slang/slang.h"
#include <cassert>
#include <cstdint>
#include <format>
#include <string_view>

#if defined(LOG_ERRORS)
#include "Modules/console.hpp"
#endif
#include <string>

#include "tl/expected.hpp"
#include <vulkan/vulkan.h>

using ErrorCode = int32_t;
using ErrorLevel = uint16_t;

struct [[nodiscard]] Error {
  std::string message;
  std::string backtrace;
  ErrorCode code = 0; // Default to 0 for success, negative for errors

  template <typename T>
  explicit Error(const tl::expected<T, Error> &result)
    requires(!std::same_as<std::remove_cvref_t<T>, Error>)
  {
    assert(!result.has_value());
    *this = result.error();
  }

  Error() = default;
  explicit Error(std::string message, std::string backtrace, ErrorCode code)
      : message(std::move(message)), backtrace(std::move(backtrace)),
        code(code) {}

  [[nodiscard]] auto ToString() const -> std::string;

  static auto SetupTraceback() -> void;

  static auto Create(const std::string &message, ErrorCode code = -1, // NOLINT
                     ErrorLevel level = 0U) -> Error;
  static auto Create(const std::string_view &message,
                     ErrorCode code = -1, // NOLINT
                     ErrorLevel level = 0U) -> Error;

  static auto Create(const char *message, ErrorCode code = -1,
                     ErrorLevel level = 0U) -> Error;

  static auto Success() -> Error;

  static auto IsError(const Error &error) -> bool;
  [[nodiscard]] auto IsError() const -> bool;
  static auto IsError(const tl::expected<void, Error> &expected) -> bool;
  template <typename T>
  static auto IsError(const tl::expected<T, Error> &expected) -> bool {
    return !expected.has_value();
  }
  static auto IsError(SlangResult result) -> bool;
  static auto IsSuccess(const Error &error) -> bool;
  [[nodiscard]] auto IsSuccess() const -> bool;

  static auto Create(VkResult result) -> Error;

  // NOLINTNEXTLINE
  static auto Create(SlangResult result, ErrorLevel level = 0) -> Error;

  static auto Create(Slang::ComPtr<slang::IBlob> &diagnosticsBlob,
                     ErrorLevel level = 0) -> Error;

  static auto Unexpected(const std::string &message, ErrorCode code = -1)
      -> tl::unexpected<Error>;

  static auto Unexpected(VkResult result) -> tl::unexpected<Error>;

  template <typename... Args>
  static auto Unexpectedf(std::string_view format, Args &&...args) {
    return tl::unexpected<Error>(
        Create(std::vformat(format,
                            std::make_format_args(std::forward<Args>(args)...)),
               -1, 1));
  }

  template <typename... Args>
  static auto Createf(std::string_view format, Args &&...args) -> Error {
    return Create(std::vformat(format, std::make_format_args(
                                           std::forward<Args>(args)...)),
                  -1, 1);
  }

  template <typename T>
  static auto Create(SlangResult result,
                     Slang::ComPtr<slang::IBlob> diagnosticsBlob,
                     T *output = nullptr) -> Error {
    if (diagnosticsBlob != nullptr && diagnosticsBlob.readRef() != nullptr) {
      return Create(diagnosticsBlob);
    }
    if (IsError(result)) {
      return Create(result, 1);
    }
    if (output == nullptr) {
      return Create("Output pointer is null", -1, 1);
    }
    return Success();
  }

  [[nodiscard]] auto AsUnexpected() const -> tl::unexpected<Error>;
};

// template <typename T> using Result = tl::expected<T, Error>;
template <class T> struct Result : tl::expected<T, Error> {
  using tl::expected<T, Error>::expected;

  Result(Error err) // NOLINT
      : tl::expected<T, Error>(tl::unexpected<Error>(std::move(err))) {}
};

// NOLINTNEXTLINE
#define CHECK_ERR(expr)                                                        \
  {                                                                            \
    auto error = (expr);                                                       \
    if (Error::IsError(error)) {                                               \
      return error;                                                            \
    }                                                                          \
  }

// NOLINTNEXTLINE
#define CHECK_RES(expr)                                                        \
  ({                                                                           \
    auto &&_result = (expr);                                                   \
    if (!_result.has_value()) {                                                \
      return _result.error();                                                  \
    }                                                                          \
    std::move(_result.value());                                                \
  })