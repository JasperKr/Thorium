#include "thread.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/renderThread.hpp"
#include "Modules/console.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include "Wrap/lua_data.hpp"
#include "Wrap/wrap.hpp"
#include "event.hpp"
#include <mutex>
#include <thread>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "channel.hpp"

namespace Threading {

inline auto GetThreadStatusChannel() -> Ref<Channel> & {
  static auto threadStatusChannel = Ref<Channel>::Make();
  return threadStatusChannel;
}

auto Thread::Create(const std::string &script) -> Ref<Thread> {
  auto thread = Ref<Thread>::Make();
  thread->script = script;
  return thread;
}

auto Thread::Run(Thread *thread,
                 const std::vector<LuaWrap::Data::LuaType> &launchArguments,
                 int count) -> void {
  lua_State *state = luaL_newstate();
  luaL_openlibs(state);

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

  LuaWrap::RegisterModules(state);

  auto err =
      Graphics::Threading::Initialize(*Graphics::GetCurrentGraphicsContext());

  if (Error::IsError(err)) {
    PrintAlways("Error initializing graphics thread module: {}", err.message);
    {
      std::lock_guard<std::mutex> lock(thread->statusMutex);
      thread->status = ThreadStatus::Error;
      thread->errorMessage = err.message;
    }
    Event::Push(Event::Event{
        .Name = "threaderror",
        .Values = {thread->errorMessage},
    });
    lua_close(state);
    return;
  }

  bool isPath = (thread->script.size() > 4 &&
                 (thread->script.substr(thread->script.size() - 4) == ".lua" ||
                  thread->script.substr(thread->script.size() - 3) == ".luac"));

  PrintAlways("Starting thread with script: {}",
              isPath ? thread->script : "[inline script]");

  {
    std::lock_guard<std::mutex> lock(thread->statusMutex);
    thread->status = ThreadStatus::Running;
  }

  int result = 0;

  if (isPath) {
    result = luaL_loadfile(state, thread->script.c_str());
  } else {
    result = luaL_loadstring(state, thread->script.c_str());
  }

  PrintAlways("Thread script load result: {}", result);

  if (result == LUA_OK) {
    auto error = LuaWrap::PushVarargs(state, launchArguments, count);

    if (Error::IsError(error)) {
      PrintAlways("Error pushing launch arguments: {}", error.message);

      {
        std::lock_guard<std::mutex> lock(thread->statusMutex);
        thread->status = ThreadStatus::Error;
        thread->errorMessage = error.message;
      }

      Event::Push(Event::Event{
          .Name = "threaderror",
          .Values = {thread->errorMessage},
      });

      lua_close(state);
      return;
    }

    result = lua_pcall(state, count, 0, 0);
  }

  if (Graphics::Threading::CurrentRenderThreadInfo.get() != nullptr) {
    {
      std::lock_guard<std::mutex> lock(
          Graphics::Threading::CurrentRenderThreadInfo->availabilityMutex);
      Graphics::Threading::CurrentRenderThreadInfo->currentlyRecording = false;
    }
    Graphics::Threading::CurrentRenderThreadInfo->availabilityCV.notify_all();
  }

  if (result != LUA_OK) {
    const auto *luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1);

    PrintAlways("Thread encountered an error: {}", (luaErrorMessage != nullptr)
                                                       ? luaErrorMessage
                                                       : "Unknown Lua error");

    {
      std::lock_guard<std::mutex> lock(thread->statusMutex);
      thread->status = ThreadStatus::Error;
      thread->errorMessage =
          (luaErrorMessage != nullptr) ? luaErrorMessage : "Unknown Lua error";
    }

    PrintAlways("Pushing thread error event to main thread.");

    Event::Push(Event::Event{
        .Name = "threaderror",
        .Values = {thread->errorMessage},
    });
  }

  PrintAlways("Thread finished execution.");

  {
    std::lock_guard<std::mutex> lock(thread->statusMutex);
    thread->status = ThreadStatus::Stopped;
  }

  lua_close(state);
}

auto Thread::Start(const std::vector<LuaWrap::Data::LuaType> &launchArguments,
                   int count) -> void {
  handle = std::thread(Threading::Thread::Run, this, launchArguments, count);
}

auto Thread::GetStatus() const -> ThreadStatus {
  std::lock_guard<std::mutex> lock(statusMutex);
  return status;
}

auto Thread::Stop() -> void {
  if (status != ThreadStatus::Running) {
    return;
  }

  if (handle.joinable()) {
    handle.join();
  }

  status = ThreadStatus::Stopped;
}

auto Thread::GetErrorMessage() const -> std::string {
  std::lock_guard<std::mutex> lock(statusMutex);
  return errorMessage;
}

} // namespace Threading