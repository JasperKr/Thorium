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
  auto operator+(ConsoleColor color) -> ColoredPrinter & {
    this->color = color;
    return *this;
  }

  // Set a new color
  auto operator+=(ConsoleColor color) -> ColoredPrinter & {
    this->color = color;
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

inline void PrintAlways(const std::string &message) {
  std::cout << ColorText("[ALWAYS] ", ConsoleColor::BrightWhite) << message
            << '\n';
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintAlways(std::string_view format, Args &&...args) {
  std::cout << ColorText("[ALWAYS] ", ConsoleColor::BrightWhite)
            << std::vformat(format, std::make_format_args(args...)) << '\n';
}

inline void PrintDebug(const std::string &message) {
  if (LogLevel::Debug >= CurrentLogLevel) {
    std::cout << ColorText("[DEBUG] ", ConsoleColor::Cyan) << message << '\n';
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintDebug(std::string_view format, Args &&...args) {
  if (LogLevel::Debug >= CurrentLogLevel) {
    std::cout << ColorText("[DEBUG] ", ConsoleColor::Cyan)
              << std::vformat(format, std::make_format_args(args...)) << '\n';
  }
}

inline void PrintInfo(const std::string &message) {
  if (LogLevel::Info >= CurrentLogLevel) {
    std::cout << ColorText("[INFO] ", ConsoleColor::Green) << message << '\n';
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintInfo(std::string_view format, Args &&...args) {
  if (LogLevel::Info >= CurrentLogLevel) {
    std::cout << ColorText("[INFO] ", ConsoleColor::Green)
              << std::vformat(format, std::make_format_args(args...)) << '\n';
  }
}

inline void PrintLibrary(const std::string &message) {
  if (LogLevel::Info >= CurrentLogLevel) {
    std::cout << ColorText("[LIBRARY] ", ConsoleColor::Blue) << message << '\n';
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintLibrary(std::string_view format, Args &&...args) {
  if (LogLevel::Info >= CurrentLogLevel) {
    std::cout << ColorText("[LIBRARY] ", ConsoleColor::Blue)
              << std::vformat(format, std::make_format_args(args...)) << '\n';
  }
}

inline void PrintWarning(const std::string &message) {
  if (LogLevel::Warning >= CurrentLogLevel) {
    std::cout << ColorText("[WARNING] ", ConsoleColor::Yellow) << message
              << '\n';
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintWarning(std::string_view format, Args &&...args) {
  if (LogLevel::Warning >= CurrentLogLevel) {
    std::cout << ColorText("[WARNING] ", ConsoleColor::Yellow)
              << std::vformat(format, std::make_format_args(args...)) << '\n';
  }
}

inline void PrintError(const std::string &message) {
  if (LogLevel::Error >= CurrentLogLevel) {
    std::cout << ColorText("[ERROR] ", ConsoleColor::Red) << message << '\n';
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintError(std::string_view format, Args &&...args) {
  if (LogLevel::Error >= CurrentLogLevel) {
    std::cout << ColorText("[ERROR] ", ConsoleColor::Red)
              << std::vformat(format, std::make_format_args(args...)) << '\n';
  }
}

inline void PrintFatal(const std::string &message) {
  if (LogLevel::Fatal >= CurrentLogLevel) {
    std::cout << ColorText("[FATAL] ", ConsoleColor::Magenta) << message
              << '\n';
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintFatal(std::string_view format, Args &&...args) {
  if (LogLevel::Fatal >= CurrentLogLevel) {
    std::cout << ColorText("[FATAL] ", ConsoleColor::Magenta)
              << std::vformat(format, std::make_format_args(args...)) << '\n';
  }
}