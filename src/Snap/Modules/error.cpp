#include "error.hpp"

#include "slang/slang-com-ptr.h"
#include "slang/slang.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <unordered_map>

#if defined(LOG_ERRORS)
#include "Modules/console.hpp"
#endif
#include <string>

#include "tl/expected.hpp"
#include <vulkan/vulkan.h>

#if __cpp_lib_stacktrace
#include <vector>
#define STD_STACKTRACE_SUPPORTED 1
#include <stacktrace>
#endif

#ifndef STD_STACKTRACE_SUPPORTED
#if defined(_WIN32)
#include <windows.h>

#include <dbghelp.h>
#elif defined(__linux__) || defined(__APPLE__) && defined(__MACH__)
#include <cstdlib>
#include <execinfo.h>
#endif
#endif

inline auto CleanupTracebackLine(const std::string &line) -> std::string {
  // Cut off each line at +0x
  size_t plusPos = line.find("+0x");
  std::string sanitizedLine;

  if (plusPos != std::string::npos) {
    sanitizedLine = line.substr(0, plusPos) + "\n";
  } else {
    sanitizedLine = line + "\n";
  }

  // Replace backslashes with forward slashes
  for (char &atIndex : sanitizedLine) {
    if (atIndex == '\\') {
      atIndex = '/';
    }
  }

  // Split before and after "snap!"
  const std::string splitKeyword = "snap!";
  size_t SnapPos = sanitizedLine.find(splitKeyword);
  std::string inFunction = "Unknown";

  if (SnapPos != std::string::npos) {
    auto inFunctionPos = SnapPos + splitKeyword.length();
    auto inFunctionCount =
        sanitizedLine.length() - inFunctionPos - 1; // -1 for newline
    inFunction =
        sanitizedLine.substr(SnapPos + splitKeyword.length(), inFunctionCount);
    sanitizedLine = sanitizedLine.substr(0, SnapPos)
                        .append("in function \'")
                        .append(inFunction)
                        .append("\'\n");
  }

  // Sanitize paths by removing everything before src/
  size_t srcPos = sanitizedLine.find("src/");
  if (srcPos != std::string::npos) {
    sanitizedLine = sanitizedLine.substr(srcPos);
  }

  // Remove (line) and change to :line
  size_t linePos = sanitizedLine.find('(');
  if (linePos != std::string::npos) {
    size_t endLinePos = sanitizedLine.find(')', linePos);
    if (endLinePos != std::string::npos) {
      std::string lineNumber =
          sanitizedLine.substr(linePos + 1, endLinePos - linePos - 1);

      auto stringAfterLineNumber = sanitizedLine.substr(endLinePos + 1);
      sanitizedLine = sanitizedLine.substr(0, linePos)
                          .append(":")
                          .append(lineNumber)
                          .append(stringAfterLineNumber);
    }
  }

  return "\t" + sanitizedLine;
}

inline auto GetStackTrace(uint32_t level = 0) -> std::string {
  const int MaxStackDepth = 64;

#ifndef STD_STACKTRACE_SUPPORTED
  const int MaxSymbolLength = 256;
  std::array<void *, MaxStackDepth> stack = {};

#if defined(_WIN32)
  unsigned short frames =
      CaptureStackBackTrace(2 + level, MaxStackDepth, stack.data(), nullptr);
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
#else
  std::string trace;
  std::stacktrace stacktrace =
      std::stacktrace::current(2 + level, MaxStackDepth);
  trace = std::to_string(stacktrace);

  std::string keyword = "!main";
  size_t pos = trace.find(keyword);
  if (pos != std::string::npos) {
    size_t endOfLine = trace.find('\n', pos);
    if (endOfLine != std::string::npos) {
      trace = trace.substr(0, endOfLine);
    }
  }

  std::vector<std::string> lines;
  size_t start = 0;
  size_t end = trace.find('\n');

  while (end != std::string::npos) {
    lines.push_back(trace.substr(start, end - start));
    start = end + 1;
    end = trace.find('\n', start);
  }

  std::string sanitizedTrace;

  for (const auto &line : lines) {
    sanitizedTrace += CleanupTracebackLine(line);
  }

  return sanitizedTrace;
#endif
  return "Unable to get stack trace on this platform.";
}

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

const std::unordered_map<SlangResult, std::string> SlangResultToStringMap = {
    {SLANG_OK, "SLANG_OK"},                                 // NOLINT
    {SLANG_FAIL, "SLANG_FAIL"},                             // NOLINT
    {SLANG_E_NOT_IMPLEMENTED, "SLANG_E_NOT_IMPLEMENTED"},   // NOLINT
    {SLANG_E_OUT_OF_MEMORY, "SLANG_E_OUT_OF_MEMORY"},       // NOLINT
    {SLANG_E_INVALID_ARG, "SLANG_E_INVALID_ARG"},           // NOLINT
    {SLANG_E_BUFFER_TOO_SMALL, "SLANG_E_BUFFER_TOO_SMALL"}, // NOLINT
    {SLANG_E_NOT_FOUND, "SLANG_E_NOT_FOUND"},               // NOLINT
};

[[nodiscard]] auto Error::ToString() const -> std::string {
  std::ostringstream oss;
  if (code >= 0) {
    oss << "Success: " << message;
    return oss.str();
  }
  oss << message << "\n(code " << code << ")";
  if (!backtrace.empty()) {
    oss << "\nBacktrace:\n" << backtrace;
  }
  return oss.str();
}

auto Error::SetupTraceback() -> void {
#ifndef STD_STACKTRACE_SUPPORTED
#if defined(_WIN32)
  SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
  SymInitialize(GetCurrentProcess(), nullptr, TRUE);
#endif
#endif
}

auto Error::Create(const std::string &message, int32_t code, uint32_t level)
    -> Error {
  Error err = Error{
      .message = message, .backtrace = GetStackTrace(level), .code = code};
#if defined(LOG_ERRORS)
  if (code < 0) {
    PrintError("{}", err.ToString());
  }
#endif
  return err;
}

auto Error::Create(const char *message, int32_t code) -> Error {
  return Create(std::string(message), code, 1);
}

auto Error::Success() -> Error {
  return {.message = "", .backtrace = "", .code = 0};
}

auto Error::IsError(const Error &error) -> bool { return error.code < 0; }
[[nodiscard]] auto Error::IsError() const -> bool { return code < 0; }
auto Error::IsError(const tl::expected<void, Error> &expected) -> bool {
  return !expected.has_value();
}

auto Error::IsError(const SlangResult result) -> bool {
  return SLANG_FAILED(result);
}
auto Error::IsSuccess(const Error &error) -> bool { return error.code >= 0; }
[[nodiscard]] auto Error::IsSuccess() const -> bool { return code >= 0; }

auto Error::Create(VkResult result) -> Error {
  if (result == VK_SUCCESS) {
    return Success();
  }

  return Create(VkResultToString(result), result, 1);
}

// NOLINTNEXTLINE
auto Error::Create(SlangResult result, uint32_t level) -> Error {
  if (SLANG_SUCCEEDED(result)) {
    return Success();
  }

  auto len = strlen(slang::getLastInternalErrorMessage());
  if (len > 0) {
    return Create(slang::getLastInternalErrorMessage(),
                  static_cast<int32_t>(result), level + 1);
  }

  /*
  Severity | Facility | Code
  ---------|----------|-----
  31       |    30-16 | 15-0

  Severity - 1 fail, 0 is success - as SlangResult is signed 32 bits, means
  negative number indicates failure. Facility is where the error originated
  from. Code is the code specific to the facility.

  Result codes have the following styles,
  1) SLANG_name
  2) SLANG_s_f_name
  3) SLANG_s_name

  where s is S for success, E for error
  f is the short version of the facility name

  Style 1 is reserved for SLANG_OK and SLANG_FAIL as they are so commonly used.

  It is acceptable to expand 'f' to a longer name to differentiate a name or
  drop if unique without it. ie for a facility 'DRIVER' it might make sense to
  have an error of the form SLANG_E_DRIVER_OUT_OF_MEMORY
  */

  // Check if the result is in the predefined map
  auto errStrIterator = SlangResultToStringMap.find(result);
  if (errStrIterator != SlangResultToStringMap.end()) {
    return Create(errStrIterator->second, static_cast<int32_t>(result),
                  level + 1);
  }

  auto errorBit = (static_cast<uint32_t>(result) >> 31U) & 0x1U; // NOLINT
  auto facilityBits = SLANG_GET_RESULT_FACILITY(result);         // NOLINT
  auto codeBits = SLANG_GET_RESULT_CODE(result);                 // NOLINT

  std::ostringstream oss;
  oss << "SlangResult Error - Severity: "
      << (errorBit == 1U ? "Error" : "Success")
      << ", Facility: " << facilityBits << ", Code: " << codeBits;
  return Create(oss.str(), static_cast<int32_t>(result), level + 1);
}

auto Error::Create(Slang::ComPtr<slang::IBlob> &diagnosticsBlob, uint32_t level)
    -> Error {
  if (diagnosticsBlob.readRef() == nullptr) {
    return Success();
  }

  // NOLINTNEXTLINE
  return Create((const char *)diagnosticsBlob->getBufferPointer(), -1,
                level + 1);
}

auto Error::Unexpected(const std::string &message, int32_t code)
    -> tl::unexpected<Error> {
  return tl::unexpected<Error>(Create(message, code, 1));
}

auto Error::Unexpected(VkResult result) -> tl::unexpected<Error> {
  return tl::unexpected<Error>(Create(result));
}

[[nodiscard]] auto Error::AsUnexpected() const -> tl::unexpected<Error> {
  return tl::unexpected<Error>(*this);
};
