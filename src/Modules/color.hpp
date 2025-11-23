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

auto GetColorCode(ConsoleColor color) -> std::string {
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

auto ColorText(const std::string &text, ConsoleColor color) -> std::string {
  return GetColorCode(color) + text + GetColorCode(ConsoleColor::Reset);
}