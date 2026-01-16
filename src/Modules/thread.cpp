#include "thread.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Wrap/wrap.hpp"
#include "event.hpp"
#include <mutex>
#include <optional>
#include <thread>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Threading {

inline auto GetThreadStatusChannel() -> Channel & {
  static Channel threadStatusChannel;
  return threadStatusChannel;
}

auto Thread::Create(const std::string &script) -> Ref<Thread> {
  auto thread = Ref<Thread>::Make();
  thread->script = script;
  return thread;
}

auto Thread::Run(Thread *thread, const EncodedLuaData &launchArguments,
                 int count) -> void {
  lua_State *state = luaL_newstate();
  luaL_openlibs(state);

  bool isPath = (thread->script.size() > 4 &&
                 (thread->script.substr(thread->script.size() - 4) == ".lua" ||
                  thread->script.substr(thread->script.size() - 3) == ".luac"));

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

  if (result == LUA_OK) {
    auto error = LuaWrap::PushVarargsFromString(state, launchArguments, count);

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

  if (result != LUA_OK) {
    const auto *luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1);

    {
      std::lock_guard<std::mutex> lock(thread->statusMutex);
      thread->status = ThreadStatus::Error;
      thread->errorMessage =
          (luaErrorMessage != nullptr) ? luaErrorMessage : "Unknown Lua error";
    }

    Event::Push(Event::Event{
        .Name = "threaderror",
        .Values = {thread->errorMessage},
    });
  }

  {
    std::lock_guard<std::mutex> lock(thread->statusMutex);
    thread->status = ThreadStatus::Stopped;
  }
}

auto Thread::Start(const EncodedLuaData &launchArguments, int count) -> void {
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

auto Channel::Push(const EncodedLuaData &message) -> void {
  std::lock_guard<std::mutex> lock(mutex);
  messages.push(message);
}

auto Channel::Pop() -> std::optional<EncodedLuaData> {
  std::lock_guard<std::mutex> lock(mutex);
  if (messages.empty()) {
    return std::nullopt;
  }
  EncodedLuaData message = messages.front();
  messages.pop();
  return message;
}

auto Channel::Peek() const -> std::optional<EncodedLuaData> {
  std::lock_guard<std::mutex> lock(mutex);
  if (messages.empty()) {
    return std::nullopt;
  }
  return messages.front();
}

auto Channel::GetCount() const -> size_t {
  std::lock_guard<std::mutex> lock(mutex);
  return messages.size();
}

auto Channel::Demand() -> EncodedLuaData {
  std::unique_lock<std::mutex> lock(mutex);
  condition.wait(lock, [this]() -> bool { return !messages.empty(); });
  EncodedLuaData message = messages.front();
  messages.pop();
  return message;
}

} // namespace Threading