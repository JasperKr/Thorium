#include "Modules/console.hpp"
#include "Modules/errorhandler.hpp"
#include "loop.hpp"
#include <string>

#include "Modules/event.hpp"

auto main() -> int {
  auto err = MainLoop();
  if (Error::IsError(err)) {
    PrintFatal(
        ColorText(err.message, ConsoleColor::Yellow) +
        "\nCode: " + ColorText(std::to_string(err.code), ConsoleColor::Green) +
        "\nTraceback:\n" + ColorText(err.backtrace, ConsoleColor::Cyan) + "\n");

    ErrorHandlerMainLoop();

    return Event::ExitCode != 0 ? Event::ExitCode : err.code;
  }

  return Event::ExitCode;
}