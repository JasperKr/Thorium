#pragma once

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

enum class LogLevel : uint8_t {
  Debug,   // Detailed debug information
  Info,    // General information about application
  Warning, // Indications of potential issues
  Error,   // Serious issues that need attention
  Fatal    // Critical errors causing application termination
};

#ifdef NDEBUG
// NOLINTNEXTLINE
inline LogLevel CurrentLogLevel = LogLevel::Warning;
#else
// NOLINTNEXTLINE
inline LogLevel CurrentLogLevel = LogLevel::Debug;
#endif

inline auto SetLogLevel(LogLevel level) -> void { CurrentLogLevel = level; }

enum class ConsoleColor : uint8_t {
  Red,         // errors
  Green,       // info
  Blue,        // library messages
  Yellow,      // warnings
  Cyan,        // debug
  Magenta,     // fatal
  BrightWhite, // always
  White,       // general
  Reset        // reset to default
};

auto inline GetColorCode(ConsoleColor color) -> std::string {
  switch (color) {
  case ConsoleColor::Red:
    return "\033[31m";
  case ConsoleColor::Green:
    return "\033[32m";
  case ConsoleColor::Blue:
    return "\033[34m";
  case ConsoleColor::Yellow:
    return "\033[33m";
  case ConsoleColor::Cyan:
    return "\033[36m";
  case ConsoleColor::Magenta:
    return "\033[35m";
  case ConsoleColor::BrightWhite:
    return "\033[97m";
  case ConsoleColor::White:
    return "\033[37m";
  default:
    return "\033[0m";
  }
}

auto inline ColorText(const std::string &text, ConsoleColor color)
    -> std::string {
  return GetColorCode(color) + text + GetColorCode(ConsoleColor::Reset);
}

struct ColoredPrinter {
public:
  // Add text with the current color
  auto operator+(const std::string &text) -> ColoredPrinter & {
    accumulatedText += ColorText(text, color);
    return *this;
  }

  // Add text with the current color
  auto operator+=(const std::string &text) -> ColoredPrinter & {
    accumulatedText += ColorText(text, color);
    return *this;
  }

  // Add text with the current color
  auto operator+(const char *text) -> ColoredPrinter & {
    accumulatedText += ColorText(std::string(text), color);
    return *this;
  }

  // Add text with the current color
  auto operator+=(const char *text) -> ColoredPrinter & {
    accumulatedText += ColorText(std::string(text), color);
    return *this;
  }

  // Set a new color
  auto operator+(ConsoleColor newColor) -> ColoredPrinter & {
    this->color = newColor;
    return *this;
  }

  // Set a new color
  auto operator+=(ConsoleColor newColor) -> ColoredPrinter & {
    this->color = newColor;
    return *this;
  }

  // Add Text and reset color
  auto operator*(std::string &text) -> ColoredPrinter & {
    accumulatedText += GetColorCode(ConsoleColor::Reset) + text;
    return *this;
  }

  // Add Text and reset color
  auto operator*=(std::string &text) -> ColoredPrinter & {
    accumulatedText += GetColorCode(ConsoleColor::Reset) + text;
    return *this;
  }

  auto Print() const -> void { std::cout << accumulatedText; }
  auto operator()() const -> void { Print(); }

private:
  ConsoleColor color = ConsoleColor::Reset;
  std::string accumulatedText;
};

struct IndentedPrinter {
public:
  explicit IndentedPrinter(uint32_t indentLevel = 0)
      : indentLevel(indentLevel) {}
  auto operator+(const std::string &text) -> IndentedPrinter & {
    indentLevel++;
    accumulatedText += GetTabs() + text + "\n";
    return *this;
  }
  auto operator+=(const std::string &text) -> IndentedPrinter & {
    indentLevel++;
    accumulatedText += GetTabs() + text + "\n";
    return *this;
  }
  auto operator-(const std::string &text) -> IndentedPrinter & {
    indentLevel = (indentLevel > 0) ? indentLevel - 1 : 0;
    accumulatedText += GetTabs() + text + "\n";
    return *this;
  }
  auto operator-=(const std::string &text) -> IndentedPrinter & {
    indentLevel = (indentLevel > 0) ? indentLevel - 1 : 0;
    accumulatedText += GetTabs() + text + "\n";
    return *this;
  }
  auto operator*(const std::string &text) -> IndentedPrinter & {
    accumulatedText += GetTabs() + text + "\n";
    return *this;
  }
  auto operator*=(const std::string &text) -> IndentedPrinter & {
    accumulatedText += GetTabs() + text + "\n";
    return *this;
  }
  auto Print() const -> void { std::cout << accumulatedText; }
  auto operator()() const -> void { Print(); }
  [[nodiscard]] auto str() const -> std::string { return accumulatedText; }
  void Indent() { ++indentLevel; }
  void UnIndent() {
    if (indentLevel > 0) {
      --indentLevel;
    }
  }
  void Inline() {
    doTabs = false;
    // Remove last newline if exists
    if (!accumulatedText.empty() && accumulatedText.back() == '\n') {
      accumulatedText.pop_back();
    }
  }

private:
  size_t indentLevel;
  std::string accumulatedText;
  bool doTabs = true;

  [[nodiscard]] auto GetTabs() -> std::string {
    if (indentLevel == 0 || !doTabs) {
      doTabs = true;
      return "";
    }
    // NOLINTNEXTLINE
    return std::string(indentLevel * 2, ' ');
  }
};

// #define LOG_TO_FILE

#ifdef LOG_TO_FILE
#include <fstream>
#include <mutex>
// NOLINTBEGIN
static std::mutex loggingMutex;
static std::ofstream logFile("Snap.log", std::ios::out | std::ios::app);
// NOLINTEND
#define OBTAIN_LOG_LOCK std::lock_guard<std::mutex> lock(loggingMutex);
#define OUT logFile
#define END                                                                    \
  '\n';                                                                        \
  logFile.flush();
#else
#define OBTAIN_LOG_LOCK
#define OUT std::cout
#define END '\n';
#endif

inline void PrintAlways(const std::string &message) {
  OBTAIN_LOG_LOCK
  OUT << ColorText("[ALWAYS] ", ConsoleColor::BrightWhite) << message << END
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintAlways(std::string_view format, Args &&...args) {
  OBTAIN_LOG_LOCK
  OUT << ColorText("[ALWAYS] ", ConsoleColor::BrightWhite)
      << std::vformat(format, std::make_format_args(args...)) << END
}

inline void PrintDebug(const std::string &message) {
  if (LogLevel::Debug >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[DEBUG] ", ConsoleColor::Cyan) << message << END
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintDebug(std::string_view format, Args &&...args) {
  if (LogLevel::Debug >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[DEBUG] ", ConsoleColor::Cyan)
        << std::vformat(format, std::make_format_args(args...)) << END
  }
}

inline void PrintInfo(const std::string &message) {
  if (LogLevel::Info >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[INFO] ", ConsoleColor::Green) << message << END
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintInfo(std::string_view format, Args &&...args) {
  if (LogLevel::Info >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[INFO] ", ConsoleColor::Green)
        << std::vformat(format, std::make_format_args(args...)) << END
  }
}

inline void PrintLibrary([[maybe_unused]] const std::string &message) {
#ifndef NDEBUG
  if (LogLevel::Info >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[LIBRARY] ", ConsoleColor::Blue) << message << END
  }
#endif
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintLibrary([[maybe_unused]] std::string_view format,
                         [[maybe_unused]] Args &&...args) {
#ifndef NDEBUG
  if (LogLevel::Info >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[LIBRARY] ", ConsoleColor::Blue)
        << std::vformat(format, std::make_format_args(args...)) << END
  }
#endif
}

inline void PrintWarning(const std::string &message) {
  if (LogLevel::Warning >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[WARNING] ", ConsoleColor::Yellow) << message << END
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintWarning(std::string_view format, Args &&...args) {
  if (LogLevel::Warning >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[WARNING] ", ConsoleColor::Yellow)
        << std::vformat(format, std::make_format_args(args...)) << END
  }
}

inline void PrintError(const std::string &message) {
  if (LogLevel::Error >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[ERROR] ", ConsoleColor::Red) << message << END
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintError(std::string_view format, Args &&...args) {
  if (LogLevel::Error >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[ERROR] ", ConsoleColor::Red)
        << std::vformat(format, std::make_format_args(args...)) << END
  }
}

inline void PrintFatal(const std::string &message) {
  if (LogLevel::Fatal >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[FATAL] ", ConsoleColor::Magenta) << message << END
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintFatal(std::string_view format, Args &&...args) {
  if (LogLevel::Fatal >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[FATAL] ", ConsoleColor::Magenta)
        << std::vformat(format, std::make_format_args(args...)) << END
  }
}