#include "Modules/errorhandler.hpp"
#include "loop.hpp"
#include <iostream>


auto main() -> int {
  auto err = MainLoop();
  if (Error::IsError(err)) {
    std::cerr << "Fatal Error: " << err.message << "\n";

    ErrorHandlerMainLoop();

    return -1;
  }

  return 0;
}