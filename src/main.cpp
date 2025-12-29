#include "Modules/console.hpp"
#include "Modules/errorhandler.hpp"
#include "loop.hpp"
#include <string>
#include <vector>

#include "Modules/event.hpp"

auto main(int argCount, char **argValues) -> int {
  std::vector<std::string> args;
  args.reserve(argCount);

  // Skip the first argument (program name)
  for (int i = 1; i < argCount; ++i) {
    args.emplace_back(argValues[i]); // NOLINT
  }

  auto err = MainLoop(args);
  if (Error::IsError(err)) {
    PrintFatal(
        ColorText(err.message, ConsoleColor::Reset) +
        "\nCode: " + ColorText(std::to_string(err.code), ConsoleColor::Green) +
        "\nTraceback:\n" + ColorText(err.backtrace, ConsoleColor::Cyan) + "\n");

    ErrorHandlerMainLoop();

    return Event::ExitCode != 0 ? Event::ExitCode : err.code;
  }

  return Event::ExitCode;
}