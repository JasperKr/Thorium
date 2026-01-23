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

auto Thread::Close(ThreadStatus status, const std::string &message) -> void {
  if (state != nullptr) {
    lua_close(state);
    state = nullptr;
  }

  Graphics::Threading::Deinitialize(*Graphics::GetCurrentGraphicsContext());

  {
    std::lock_guard<std::mutex> lock(statusMutex);
    this->status = status;
    this->errorMessage = message;
  }

  if (status == ThreadStatus::Error) {
    PrintWarning("Thread encountered an error: {}", message);
    Event::Push(Event::Event{
        .Name = "threaderror",
        .Values = {message},
    });
  }
}

auto Thread::Run(Thread *thread,
                 const std::vector<LuaWrap::Data::LuaType> &launchArguments,
                 int count) -> void {
  lua_State *state = luaL_newstate();
  luaL_openlibs(state);
  thread->state = state;

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
    thread->Close(ThreadStatus::Error, err.message);
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
      thread->Close(ThreadStatus::Error, error.message);
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

    thread->Close(ThreadStatus::Error, luaErrorMessage != nullptr
                                           ? std::string(luaErrorMessage)
                                           : "Unknown Lua error occurred.");
    return;
  }

  PrintAlways("Thread finished execution.");

  thread->Close(ThreadStatus::Stopped, "");
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