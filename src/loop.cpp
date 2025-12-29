#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/render.hpp"
#include "Graphics/rendertarget.hpp"
#include "Graphics/shader.hpp"
#include "Modules/config.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"

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

constexpr auto loader = R"lua(
local success, module = xpcall(require, debug.traceback, "main")
if not success then
  print(module, 3)
end

__LOAD_SUCCESS = success
)lua";

// NOLINTNEXTLINE
static LuaWrap::LuaRef runCallback;

auto LoadLua(lua_State *state) -> Error {
  // Load src/Engine/main.lua
  PrintDebug("Loading main Lua script...");

  auto luaLoadErr = luaL_dostring(state, loader);
  if (static_cast<int>(luaLoadErr) != LUA_OK) {
    if (lua_isstring(state, -1) != 0) {
      std::string luaErrorMessage = lua_tostring(state, -1);
      lua_pop(state, 1); // Remove error message from stack
      return Error::Create("Failed to load main Lua script: " +
                           luaErrorMessage);
    }

    lua_pop(state, 1); // Remove non-string error from stack
    return Error::Create("Failed to load main Lua script: Unknown error");
  }

  // Check if module loaded successfully
  lua_getglobal(state, "__LOAD_SUCCESS");
  bool loadSuccess = lua_toboolean(state, -1) != 0;
  lua_pop(state, 1); // Remove __LOAD_SUCCESS from stack

  if (!loadSuccess) {
    return Error::Create("lua error");
  }

  // Get Thorium.run function
  lua_getglobal(state, "Thorium");
  lua_getfield(state, -1, "run");

  if (!lua_isfunction(state, -1)) {
    // If Thorium.run is not defined, load default
    PrintDebug("Thorium.run not found, loading default run function...");
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

auto MainLoop() -> Error {
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
  Error fsInitErr = Filesystem::Init(".");

  if (Error::IsError(fsInitErr)) {
    return fsInitErr;
  }

  PrintInfo("Save directory: " + Filesystem::GetSaveDirectory());

  Error fsMntErr = Filesystem::Mount(".", "/", true);
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

  auto error = Graphics::InitializeGlobalTimelineSemaphore(context);
  if (Error::IsError(error)) {
    return error;
  }

  error = Graphics::LoadBufferModule(context);
  if (Error::IsError(error)) {
    return error;
  }

  error = InitializeUniformBufferModule(context);
  if (Error::IsError(error)) {
    return error;
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

      return Error::Create(luaErrorMessage);
    }

    // returned value nil == continue, non-nil == exit with code
    if (lua_isnil(state, -1)) {
      lua_pop(state, 1); // pop nil
    } else {
      int exitCode = static_cast<int>(lua_tointeger(state, -1));
      lua_pop(state, 1); // pop exit code
      PrintInfo("Exiting main loop with code " + std::to_string(exitCode));
      lua_close(state);
      Event::ExitCode = exitCode;
      Event::MainLoopRunning = false;
    }
  }

  vkDeviceWaitIdle(context.device);

  result = FlushBufferUploads(context);
  if (Error::IsError(result)) {
    return result;
  }
  result = Graphics::UnloadBufferModule(context);
  if (Error::IsError(result)) {
    return result;
  }

  Graphics::Deinitialize(context);
  Graphics::RenderTarget::Destroy(context);

  PrintInfo("App shutdown complete.");

  return Error::Success();
}
