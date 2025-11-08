#include "Graphics/canvas.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/render.hpp"
#include "Graphics/shader.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "SDL3/SDL_events.h"
#include <iostream>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "Modules/timer.hpp"
#include "program.hpp"

auto main() -> int {
  Graphics::GetCurrentThreadIndex() = 0;
  Error::SetupTraceback();

  ApplicationConfig config = {};

  Program::Configuration(config);

  Filesystem::GetConfig().identity = config.Identity;
  Error::Error fsInitErr = Filesystem::Init(".");

  if (Error::IsError(fsInitErr)) {
    std::cerr << "Failed to initialize filesystem: " << fsInitErr.message
              << "\n";
    return -1;
  }

  std::cout << "Save directory: " << Filesystem::GetSaveDirectory() << "\n";

  Error::Error fsMntErr = Filesystem::Mount(".", "/", true);
  if (Error::IsError(fsMntErr)) {
    std::cerr << "Failed to mount filesystem: " << fsMntErr.message << "\n";
    return -1;
  }

  Graphics::GraphicsContext context = {};
  context.renderThreadCount = 1;

  const VkExtent2D dimensions = {800, 600};

  std::cout << "Initializing graphics..." << "\n";

  auto result = Graphics::Initialize(context, dimensions);
  if (Error::IsError(result)) {
    return -1;
  }

  std::cout << "Graphics initialized successfully." << "\n";

  SDL_Event event;
  bool running = true;

  Graphics::Shader::LoadModule();

  std::cout << "Loading program..." << "\n";

  Error::Error loadErr = Program::Load(context);

  if (Error::IsError(loadErr)) {
    std::cerr << "Failed to load program: " << loadErr.message << "\n";
    return -1;
  }

  std::cout << "Program loaded successfully." << "\n";

  Graphics::InitializeGraphics(context);
  Error::Error err = Graphics::SetCanvas(context, {}, nullptr);

  std::cout << "Entering main loop..." << "\n";

  while (running) {
    // Events

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        running = false;
        break;
      default:
        break;
      }
    }

    if (!running) {
      break;
    }

    Timer::Step();
    Error::Error updateErr = Program::Update(Timer::GetDelta());
    if (Error::IsError(updateErr)) {
      std::cerr << "Error::Error during update: " << updateErr.message << "\n";
      running = false;
      break;
    }
    Error::Error drawErr = Program::Draw(context);
    if (Error::IsError(drawErr)) {
      std::cerr << "Error::Error during draw: " << drawErr.message << "\n";
      running = false;
      break;
    }

    Error::Error err = Graphics::Present(context);

    if (Error::IsError(err)) {
      std::cerr << "Error::Error during presentation: " << err.message << "\n";
      running = false;
    }
  }

  std::cout << "Exiting program..." << "\n";

  Error::Error exitErr = Program::Exit(context);
  if (Error::IsError(exitErr)) {
    std::cerr << "Error::Error during program exit: " << exitErr.message
              << "\n";
  }

  std::cout << "Program exited successfully." << "\n";

  Graphics::Deinitialize(context);

  return 0;
}
