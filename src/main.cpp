#include "Modules/color.hpp"
#include "Modules/errorhandler.hpp"
#include "loop.hpp"
#include <iostream>
#include <string>

auto main() -> int {
  std::cout << _MSVC_LANG << "\n";

  auto err = MainLoop();
  if (Error::IsError(err)) {
    std::cerr << ColorText("Fatal Error.\n", ConsoleColor::Red) << "Code: "
              << ColorText(std::to_string(err.code), ConsoleColor::Green)
              << "\nMessage: " << ColorText(err.message, ConsoleColor::Yellow)
              << "\nTraceback:\n"
              << ColorText(err.backtrace, ConsoleColor::Cyan) << "\n";

    ErrorHandlerMainLoop();

    return -1;
  }

  return 0;
}