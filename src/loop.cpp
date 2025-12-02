#include "Graphics/graphics.hpp"
#include "Graphics/render.hpp"
#include "Graphics/rendertarget.hpp"
#include "Graphics/shader.hpp"
#include "Modules/config.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include <cstdint>
#include <iostream>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "program.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "Modules/event.hpp"
#include "Wrap/reference.hpp"
#include "Wrap/wrap.hpp"

constexpr auto defaultRunFunction = R"lua(
function Thorium.run()

  if Thorium.load then
    Thorium.load()
  end

  return function()
    Thorium.event.pull()

    local name, a,b,c,d,e,f = Thorium.event.pop()
    while name do
      if name == "quit" then
        if Thorium.quit then
          return Thorium.quit() or 0
        end
        return 0
      end

      if Thorium[name] then
        Thorium[name](a,b,c,d,e,f)
      end

      name, a,b,c,d,e,f = Thorium.event.pop()
    end

    Thorium.timer.step()

    local dt = Thorium.timer.getDelta()

    if Thorium.update then
      Thorium.update(dt)
    end

    if Thorium.graphics then
      if Thorium.draw then
        Thorium.draw()
      end

      Thorium.graphics.present()
    end
  end
end
)lua";

// NOLINTNEXTLINE
static LuaWrap::LuaRef runCallback;

auto LoadLua(lua_State *state) -> Error::Error {
  // Load src/Engine/main.lua
  std::cout << "Loading main Lua script..." << "\n";

  auto luaLoadErr = luaL_dofile(state, "src/Engine/main.lua");
  if (static_cast<int>(luaLoadErr) != LUA_OK) {
    std::string luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1); // Remove error message from stack
    return Error::Create("Failed to load main Lua script: " + luaErrorMessage);
  }

  // Get Thorium.run function
  lua_getglobal(state, "Thorium");
  lua_getfield(state, -1, "run");

  if (!lua_isfunction(state, -1)) {
    // If Thorium.run is not defined, load default
    std::cout << "Thorium.run not found, loading default run function..."
              << "\n";
    lua_pop(state, 2); // Remove non-function and Thorium table from stack
    if (luaL_dostring(state, defaultRunFunction) != LUA_OK) {
      std::string luaErrorMessage = lua_tostring(state, -1);
      lua_pop(state, 1); // Remove error message from stack
      return Error::Create("Failed to load default run function: " +
                           luaErrorMessage);
    }
    lua_getglobal(state, "Thorium");
    lua_getfield(state, -1, "run");
  }

  if (!lua_isfunction(state, -1)) {
    lua_pop(state, 1); // Remove non-function from stack
    return Error::Create("Thorium.run is not a function.");
  }

  // Call Thorium.run to get the main loop function
  if (lua_pcall(state, 0, 1, 0) != LUA_OK) {
    std::string luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1); // Remove error message from stack
    return Error::Create("Failed to call Thorium.run: " + luaErrorMessage);
  }

  if (!lua_isfunction(state, -1)) {
    lua_pop(state, 1); // Remove non-function from stack
    return Error::Create("Thorium.run did not return a function.");
  }

  std::cout << "Main Lua script loaded successfully." << "\n";

  runCallback = LuaWrap::LuaRef::FromStack(state);

  return Error::Success();
}

auto MainLoop() -> Error::Error {
  Graphics::GetCurrentThreadIndex() = 0;
  Error::SetupTraceback();

  std::cout << "Initializing Lua state..." << "\n";
  std::flush(std::cout);

  lua_State *state = luaL_newstate();
  luaL_openlibs(state);

  std::cout << "Registering Lua modules..." << "\n";

  LuaWrap::RegisterModules(state);

  std::cout << "Lua modules registered." << "\n";

  auto configResult = Config::Configure(state);

  if (Error::IsError(configResult)) {
    return configResult.error();
  }

  auto config = configResult.value();

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

  std::cout << "Initializing graphics..." << "\n";

  auto result = Graphics::Initialize(context, config);
  if (Error::IsError(result)) {
    return result;
  }

  std::cout << "Graphics initialized successfully." << "\n";

  bool running = true;

  Graphics::Shader::LoadModule();
  auto rendertargetLoadError = Graphics::RenderTarget::Load(context);

  if (Error::IsError(rendertargetLoadError)) {
    return rendertargetLoadError;
  }

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
      return err;
    }
  }

  Graphics::SetCurrentGraphicsContext(&context);

  auto luaLoadErr = LoadLua(state);

  if (Error::IsError(luaLoadErr)) {
    return luaLoadErr;
  }

  std::cout << "Entering main loop..." << "\n";

  lua_getglobal(state, "debug");
  lua_getfield(state, -1, "traceback");
  lua_remove(state, -2); // remove debug table
  auto tracebackIndex = lua_gettop(state);

  while (Event::MainLoopRunning) {
    runCallback.push();

    if (lua_pcall(state, 0, 1, tracebackIndex) != LUA_OK) {
      std::string luaErrorMessage = lua_tostring(state, -1);
      lua_pop(state, 1); // Remove error message from stack
      Event::MainLoopRunning = false;
      Event::ExitCode = 1;

      return Error::Create("Error during main loop: " + luaErrorMessage);
    }

    // returned value nil == continue, non-nil == exit with code
    if (lua_isnil(state, -1)) {
      lua_pop(state, 1); // pop nil
    } else {
      int exitCode = static_cast<int>(lua_tointeger(state, -1));
      lua_pop(state, 1); // pop exit code
      std::cout << "Exiting main loop with code " << exitCode << "\n";
      Event::ExitCode = exitCode;
      Event::MainLoopRunning = false;
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
