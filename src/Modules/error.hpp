#pragma once

#define LOG_ERRORS 1

#include <array>
#include <cstdint>
#if defined(LOG_ERRORS)
#include <iostream>
#endif
#include <string>
#define VK_NO_PROTOTYPES
#include "tl/expected.hpp"
#include <vulkan/vulkan.h>

#if defined(_WIN32)
#include <windows.h>

#include <dbghelp.h>
#elif defined(__linux__) || defined(__APPLE__) && defined(__MACH__)
#include <cstdlib>
#include <execinfo.h>
#endif

namespace Error {
struct Error {
  std::string message;
  int32_t code;
  std::string backtrace;
};

inline auto SetupTraceback() -> void {
#if defined(_WIN32)
  SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
  SymInitialize(GetCurrentProcess(), nullptr, TRUE);
#endif
}

inline auto GetStackTrace() -> std::string {
  const int MaxStackDepth = 64;
  const int MaxSymbolLength = 256;
  std::array<void *, MaxStackDepth> stack = {};

#if defined(_WIN32)
  unsigned short frames =
      CaptureStackBackTrace(0, MaxStackDepth, stack.data(), nullptr);
  std::string trace;
  HANDLE process = GetCurrentProcess();

  // NOLINTNEXTLINE
  auto *symbol = // NOLINTNEXTLINE
      (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + MaxSymbolLength, 1);
  symbol->MaxNameLen = MaxSymbolLength - 1;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

  for (unsigned short i = 0; i < frames; i++) {
    auto address = (DWORD64)(stack.at(i)); // NOLINT
    if (SymFromAddr(process, address, nullptr, symbol) != 0) {
      trace += "\tin function: ";
      trace += symbol->Name; // NOLINT
      trace += "\n";
    } else {
      trace += std::to_string(i) + ": [symbol lookup failed] - " +
               std::to_string(address) + "\n";
    }
  }

  return trace;
#elif defined(__linux__) || defined(__APPLE__) && defined(__MACH__)
  int frames = backtrace(stack.data(), MaxStackDepth);
  char **symbols = backtrace_symbols(stack.data(), frames);

  std::string trace;
  for (int i = 0; i < frames; i++) {
    // NOLINTNEXTLINE
    trace += std::string(symbols[i]) + "\n";
  }

  free(symbols); // NOLINT
  return trace;
#endif
  return "Unable to get stack trace on this platform.";
}

inline auto Create(const std::string &message, int32_t code = 1) -> Error {
  Error err =
      Error{.message = message, .code = code, .backtrace = GetStackTrace()};
#if defined(LOG_ERRORS)
  if (code != 0) {
    std::cerr << "Error: " << message << "\n"
              << "Code: " << code << "\n"
              << "Backtrace:\n"
              << err.backtrace << "\n";
  }
#endif
  return err;
}

inline auto Success() -> Error {
  return Error{.message = "", .code = 0, .backtrace = ""};
}

inline auto IsError(const Error &error) -> bool { return error.code != 0; }
inline auto IsError(const tl::expected<void, Error> &expected) -> bool {
  return !expected.has_value();
}
template <typename T>
inline auto IsError(const tl::expected<T, Error> &expected) -> bool {
  return !expected.has_value();
}

inline auto IsSuccess(const Error &error) -> bool { return error.code == 0; }

inline auto VkResultToString(int32_t result) -> std::string {
  switch (result) {
  case VK_SUCCESS:
    return "VK_SUCCESS";
  case VK_NOT_READY:
    return "VK_NOT_READY";
  case VK_TIMEOUT:
    return "VK_TIMEOUT";
  case VK_EVENT_SET:
    return "VK_EVENT_SET";
  case VK_EVENT_RESET:
    return "VK_EVENT_RESET";
  case VK_INCOMPLETE:
    return "VK_INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED:
    return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST:
    return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED:
    return "VK_ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT:
    return "VK_ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return "VK_ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER:
    return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS:
    return "VK_ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_FRAGMENTED_POOL:
    return "VK_ERROR_FRAGMENTED_POOL";
  default:
    return "UNKNOWN_VK_RESULT";
  }
}

inline auto FromVkResult(VkResult result) -> Error {
  if (result == VK_SUCCESS) {
    return Success();
  }

  return Create(VkResultToString(result), result);
}

inline auto Unexpected(const std::string &message, int32_t code = 1) {
  return tl::unexpected<Error>(Create(message, code));
}

} // namespace Error