#include "Graphics/graphics.hpp"
#include "Graphics/render.hpp"
#include "Graphics/rendertarget.hpp"
#include "Graphics/shader.hpp"
#include "Modules/config.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include <cstdint>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

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
  PrintDebug("Loading main Lua script...");

  auto luaLoadErr = luaL_dofile(state, "src/Engine/main.lua");
  if (static_cast<int>(luaLoadErr) != LUA_OK) {
    std::string luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1); // Remove error message from stack
    return Error::Create(luaErrorMessage);
  }

  // Get Thorium.run function
  lua_getglobal(state, "Thorium");
  lua_getfield(state, -1, "run");

  if (!lua_isfunction(state, -1)) {
    // If Thorium.run is not defined, load default
    PrintWarning("Thorium.run not found, loading default run function...");
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

  PrintDebug("Main Lua script loaded successfully.");

  runCallback = LuaWrap::LuaRef::FromStack(state);

  return Error::Success();
}

auto MainLoop() -> Error::Error {
  Graphics::GetCurrentThreadIndex() = 0;
  Error::SetupTraceback();

  PrintDebug("Initializing Lua state...");

  lua_State *state = luaL_newstate();
  luaL_openlibs(state);

  PrintDebug("Registering Lua modules...");

  LuaWrap::RegisterModules(state);

  PrintDebug("Lua modules registered.");

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

  PrintInfo("Save directory: " + Filesystem::GetSaveDirectory());

  Error::Error fsMntErr = Filesystem::Mount(".", "/", true);
  if (Error::IsError(fsMntErr)) {
    return fsMntErr;
  }

  PrintInfo("Source directory: " + Filesystem::GetSourceDirectory());

  Graphics::GraphicsContext context = {};
  context.renderThreadCount = 1;

  PrintDebug("Initializing graphics...");

  auto result = Graphics::Initialize(context, config);
  if (Error::IsError(result)) {
    return result;
  }

  PrintDebug("Graphics initialized successfully.");

  Graphics::SetCurrentGraphicsContext(&context);

  auto shaderModuleLoadResult = Graphics::Shader::LoadModule();

  if (Error::IsError(shaderModuleLoadResult)) {
    return shaderModuleLoadResult;
  }

  PrintDebug("Shader modules loaded successfully.");

  auto rendertargetLoadError = Graphics::RenderTarget::Load(context);

  if (Error::IsError(rendertargetLoadError)) {
    return rendertargetLoadError;
  }

  PrintDebug("Rendertargets loaded successfully.");

  result = Graphics::InitializeGraphics(context);

  if (Error::IsError(result)) {
    return result;
  }

  PrintDebug("Swapchains filled successfully.");

  for (int32_t idx = 0; idx < context.swapchainInfo.imageCount; idx++) {
    // Fill swapchain images initially
    // To make sure all further frames are waiting on vsync properly
    // and the timer does not explode due to tiny delta times
    Error::Error err = Graphics::Present(context);

    if (Error::IsError(err)) {
      return err;
    }
  }

  auto luaLoadErr = LoadLua(state);

  if (Error::IsError(luaLoadErr)) {
    return luaLoadErr;
  }

  PrintDebug("Entering main loop...");

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
      PrintInfo("Exiting main loop with code " + std::to_string(exitCode));
      Event::ExitCode = exitCode;
      Event::MainLoopRunning = false;
    }
  }

  Graphics::Deinitialize(context);
  Graphics::RenderTarget::Destroy(context);

  PrintInfo("App shutdown complete.");

  return Error::Success();
}
