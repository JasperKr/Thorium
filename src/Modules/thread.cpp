#include "thread.hpp"
#include "Modules/error.hpp"
#include <mutex>
#include <thread>

namespace Threading {

inline auto RunThread(const std::string &script) -> void {
  // Implementation for running the thread

  lua_State *state = luaL_newstate();
  luaL_openlibs(state);

  bool isPath =
      (script.size() > 4 && (script.substr(script.size() - 4) == ".lua" ||
                             script.substr(script.size() - 3) == ".luac"));

  if (isPath) {
    if (luaL_dofile(state, script.c_str()) != LUA_OK) {
      const auto *luaErrorMessage = lua_tostring(state, -1);
      lua_pop(state, 1); // Remove error message from stack
      // Handle error (e.g., log it)
      return;
    }
  }
}

auto Thread::Start(const std::string &script) -> void {
  Thread thread{};

  thread.handle = std::thread(RunThread, script);
  status = ThreadStatus::Running;
}

auto Thread::GetStatus() const -> ThreadStatus { return status; }

auto Thread::Stop() -> void {
  if (status != ThreadStatus::Running) {
    return;
  }

  if (handle.joinable()) {
    handle.join();
  }

  status = ThreadStatus::Stopped;
}

auto Channel::Push(const KeyValuePair &message) -> void {
  std::lock_guard<std::mutex> lock(mutex);
  messages.push(message);
}

auto Channel::Pop() -> Result<KeyValuePair> {
  std::lock_guard<std::mutex> lock(mutex);
  if (messages.empty()) {
    return Error::Unexpected("Channel is empty");
  }
  KeyValuePair message = messages.front();
  messages.pop();
  return message;
}

auto Channel::Peek() -> Result<KeyValuePair> {
  std::lock_guard<std::mutex> lock(mutex);
  if (messages.empty()) {
    return Error::Unexpected("Channel is empty");
  }
  return messages.front();
}

auto Channel::Demand() -> KeyValuePair {
  std::unique_lock<std::mutex> lock(mutex);
  condition.wait(lock, [this]() -> bool { return !messages.empty(); });
  KeyValuePair message = messages.front();
  messages.pop();
  return message;
}

} // namespace Threading