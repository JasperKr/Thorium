#pragma once

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

enum class ConsoleColor : uint8_t {
  Red,
  Green,
  Blue,
  Yellow,
  Cyan,
  Magenta,
  White,
  Reset
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