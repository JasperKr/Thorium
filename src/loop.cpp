#include "Graphics/graphics.hpp"
#include "Graphics/render.hpp"
#include "Graphics/shader.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "SDL3/SDL_events.h"
#include <cstdint>
#include <iostream>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "Modules/timer.hpp"
#include "program.hpp"

auto MainLoop() -> Error::Error {
  Graphics::GetCurrentThreadIndex() = 0;
  Error::SetupTraceback();

  ApplicationConfig config = {};

  auto configError = Program::Configuration(config);
  if (Error::IsError(configError)) {
    return configError;
  }

  Filesystem::GetConfig().identity = config.Identity;
  Error::Error fsInitErr = Filesystem::Init(".");

  if (Error::IsError(fsInitErr)) {
    return fsInitErr;
  }

  std::cout << "Save directory: " << Filesystem::GetSaveDirectory() << "\n";

  Error::Error fsMntErr = Filesystem::Mount(".", "/", true);
  if (Error::IsError(fsMntErr)) {
    return fsMntErr;
  }

  std::cout << "Source directory: " << Filesystem::GetSourceDirectory() << "\n";

  Graphics::GraphicsContext context = {};
  context.renderThreadCount = 1;

  const VkExtent2D dimensions = {
      .width = static_cast<uint32_t>(config.Size.width),
      .height = static_cast<uint32_t>(config.Size.height)};

  std::cout << "Initializing graphics..." << "\n";

  auto result = Graphics::Initialize(context, dimensions);
  if (Error::IsError(result)) {
    return result;
  }

  std::cout << "Graphics initialized successfully." << "\n";

  SDL_Event event;
  bool running = true;

  Graphics::Shader::LoadModule();

  std::cout << "Loading program..." << "\n";

  Error::Error loadErr = Program::Load(context);

  if (Error::IsError(loadErr)) {
    return loadErr;
  }

  std::cout << "Program loaded successfully." << "\n";

  Graphics::InitializeGraphics(context);

  for (int32_t idx = 0; idx < context.swapchainInfo.imageCount; idx++) {
    // Fill swapchain images initially
    // To make sure all further frames are waiting on vsync properly
    // and the timer does not explode due to tiny delta times
    Error::Error err = Graphics::Present(context);

    if (Error::IsError(err)) {
      std::cerr << "Error::Error during presentation: " << err.message << "\n";
      running = false;
    }
  }

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

  return Error::Success();
}
