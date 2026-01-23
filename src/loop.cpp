#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/dynamicRendering.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/info.hpp"
#include "Graphics/render.hpp"
#include "Graphics/shader.hpp"
#include "Modules/Editor/gui.hpp"
#include "Modules/config.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/thread.hpp"
#include "SDL3/SDL_cpuinfo.h"
#include <filesystem>
#include <string>
#include <vector>

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

auto LoadLua(lua_State *state, const std::vector<std::string> &launchArgs)
    -> Error {
  // Load src/Engine/main.lua
  PrintDebug("Loading main Lua script...");
  if (launchArgs.empty()) {
    return Error::Create("No launch arguments provided for Lua script.");
  }

  lua_getglobal(state, "package");
  lua_getfield(state, -1, "path");
  std::string currentPath = lua_tostring(state, -1);
  lua_pop(state, 1); // remove original path
  std::string newPath =
      Path::Join(Filesystem::GetSourceDirectory(), std::string("?.lua;")) +
      currentPath;
  lua_pushstring(state, newPath.c_str());
  lua_setfield(state, -2, "path");
  lua_pop(state, 1); // remove package table

  auto luaLoadErr =
      luaL_loadfile(state, Path::Join(Filesystem::GetSourceDirectory(),
                                      std::string("main.lua"))
                               .c_str());

  if (luaLoadErr != LUA_OK) {
    if (lua_isstring(state, -1) != 0) {
      std::string luaErrorMessage = lua_tostring(state, -1);
      lua_pop(state, 1); // Remove error message from stack
      return Error::Create(luaErrorMessage);
    }

    lua_pop(state, 1); // Remove non-string error from stack
    return Error::Create("Failed to load main Lua script: Unknown error");
  }

  for (const auto &arg : launchArgs) {
    lua_pushstring(state, arg.c_str());
  }

  // Call the loaded chunk
  if (lua_pcall(state, static_cast<int>(launchArgs.size()), 0, 0) != LUA_OK) {
    std::string luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1); // Remove error message from stack
    return Error::Create(luaErrorMessage);
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

auto MainLoop(const std::vector<std::string> &arguments) -> Error {
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

#if defined(__linux__)
  std::filesystem::path exeDir =
      std::filesystem::read_symlink("/proc/self/exe").parent_path();
#elif defined(_WIN32)
  char buffer[MAX_PATH];
  GetModuleFileNameA(NULL, buffer, MAX_PATH);
  std::filesystem::path exeDir = std::filesystem::path(buffer).parent_path();
#else
  std::filesystem::path exeDir = std::filesystem::current_path();
#endif

  if (arguments.size() == 0) {
    return Error::Create("No Lua script specified to run.");
  }

  auto sourceDirectory = Path::Sanitize(exeDir.string() + "/" + arguments[0]);
  sourceDirectory = Path::Directory(sourceDirectory);

  Filesystem::GetConfig().identity = config.Identity;
  Error fsInitErr = Filesystem::Init(".");

  if (Error::IsError(fsInitErr)) {
    return fsInitErr;
  }

  auto sourceSetError = Filesystem::SetSourceDirectory(sourceDirectory);

  if (Error::IsError(sourceSetError)) {
    return sourceSetError;
  }

  Error fsMntErr = Filesystem::Mount(".", "/", true);
  if (Error::IsError(fsMntErr)) {
    return fsMntErr;
  }

  Graphics::GraphicsContext context = {};
  context.renderThreadCount = SDL_GetNumLogicalCPUCores();

  PrintDebug("Initializing graphics...");

  auto result = Graphics::Initialize(context, config);
  if (Error::IsError(result)) {
    return result;
  }

  PrintDebug("Graphics initialized successfully.");
  PrintAlways(Graphics::Info::GetGpuInfoString(context.physicalDevice));

  Graphics::SetCurrentGraphicsContext(&context);

  PrintDebug("Loading shader modules...");

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

  auto guiLoadResult = Gui::LoadGUIState(state);

  if (Error::IsError(guiLoadResult)) {
    return guiLoadResult.error();
  }

  auto luaLoadErr = LoadLua(state, arguments);

  if (Error::IsError(luaLoadErr)) {
    return luaLoadErr;
  }

  // Engine::Scene scene;

  // auto gltfresult = glTF::LoadGltfModel(context, "assets/testscene.glb", scene);
  // if (Error::IsError(gltfresult)) {
  //   return gltfresult;
  // }

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

    PrintAlways("Frame complete; {}", lua_isnoneornil(state, -1));

    // returned value nil == continue, non-nil == exit with code
    if (lua_isnoneornil(state, -1)) {
      lua_pop(state, 1); // pop nil
    } else {
      int exitCode = static_cast<int>(lua_tointeger(state, -1));
      lua_pop(state, 1); // pop exit code
      PrintInfo("Exiting main loop with code " + std::to_string(exitCode));
      Event::ExitCode = exitCode;
      Event::MainLoopRunning = false;
    }
  }

  PrintInfo("Closing Lua state...");
  lua_close(state);

  PrintInfo("Waiting on device idle...");
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    vkDeviceWaitIdle(context.device);
  }

  PrintInfo("Deinitializing threading module...");

  // result = FlushBufferUploads(context);
  // if (Error::IsError(result)) {
  //   return result;
  // }

  Threading::UnloadModule();

  PrintInfo("Deinitializing graphics...");

  Graphics::DeInitializeUniformBufferModule(context);

  PrintInfo("Unloading buffer module...");

  result = Graphics::UnloadBufferModule(context);
  if (Error::IsError(result)) {
    return result;
  }

  PrintInfo("Deinitializing global timeline semaphore...");

  DeInitializeGlobalTimelineSemaphore(context);

  PrintInfo("Unloading shader modules...");

  Graphics::Shader::UnloadModule(context);

  PrintInfo("Destroying rendertargets...");
  Graphics::RenderTarget::Destroy(context);

  PrintInfo("Destroying graphics context...");

  Graphics::Deinitialize(context);

  PrintInfo("App shutdown complete.");

  return Error::Success();
}
