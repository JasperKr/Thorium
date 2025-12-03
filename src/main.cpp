#include "Modules/console.hpp"
#include "Modules/errorhandler.hpp"
#include "loop.hpp"
#include <iostream>
#include <string>

#include "Modules/event.hpp"

auto main() -> int {
  auto err = MainLoop();
  if (Error::IsError(err)) {
    std::cerr << ColorText("Fatal Error.\n", ConsoleColor::Red) << "Code: "
              << ColorText(std::to_string(err.code), ConsoleColor::Green)
              << "\nMessage: " << ColorText(err.message, ConsoleColor::Yellow)
              << "\nTraceback:\n"
              << ColorText(err.backtrace, ConsoleColor::Cyan) << "\n";

    ErrorHandlerMainLoop();

    return Event::ExitCode != 0 ? Event::ExitCode : err.code;
  }

  return Event::ExitCode;
}