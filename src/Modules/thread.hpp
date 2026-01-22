#pragma once

#include "Modules/console.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/lua_data.hpp"
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

// Thread system for lua scripts
// Each thread has its own lua_State and runs independently
// Threads can communicate via channels
// Channels are thread-safe queues that can hold messages
// Messages can be of any type that is serializable:
// numbers, strings, booleans, tables, but not functions or userdata except for any engine-defined types

namespace Threading {

auto UnloadModule() -> void;

enum class ThreadStatus : uint8_t { Running, Stopped, Error };

const static Type threadType = Type("Thread");

struct Thread : Object {
public:
  Thread() = default;
  Thread(const Thread &) = delete;
  Thread(Thread &&) = delete;
  auto operator=(const Thread &) -> Thread & = delete;
  auto operator=(Thread &&) -> Thread & = delete;
  ~Thread() override = default;

  static auto Create(const std::string &script) -> Ref<Thread>;

  auto Start(const std::vector<LuaWrap::Data::LuaType> &launchArguments,
             int count) -> void;
  [[nodiscard]] auto GetStatus() const -> ThreadStatus;
  [[nodiscard]] auto GetErrorMessage() const -> std::string;
  auto Stop() -> void;

  auto GetInstanceType() const -> Type const * override { return &threadType; }
  static auto GetType() -> Type const * { return &threadType; }

private:
  static auto Run(Thread *thread,
                  const std::vector<LuaWrap::Data::LuaType> &launchArguments,
                  int count) -> void;

  std::string script;
  lua_State *luaState = nullptr;
  std::thread handle;

  mutable std::mutex statusMutex;
  ThreadStatus status{ThreadStatus::Stopped};
  std::string errorMessage;
};

const static Type channelType = Type("Channel");

// Channel storage for shutdown
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::vector<Ref<struct Channel>> Channels;

struct Channel : Object {
public:
  Channel() {
    Channels.emplace_back(this);
    this->release(); // The channels vector does not own a reference
  }
  Channel(const Channel &) = delete;
  Channel(Channel &&) = delete;
  auto operator=(const Channel &) -> Channel & = delete;
  auto operator=(Channel &&) -> Channel & = delete;
  // Push a message to the channel
  auto Push(const LuaWrap::Data::LuaType &message) -> void;

  // Pop a message from the channel
  auto Pop() -> std::optional<LuaWrap::Data::LuaType>;

  // Peek at the next message without removing it
  [[nodiscard]]
  auto Peek() const -> std::optional<LuaWrap::Data::LuaType>;

  [[nodiscard]]
  auto GetCount() const -> size_t;

  // Demand a message, blocking until one is available
  auto Demand(double timeout = INFINITY)
      -> std::optional<LuaWrap::Data::LuaType>;

  auto GetInstanceType() const -> Type const * override { return &channelType; }
  static auto GetType() -> Type const * { return &channelType; }

  auto IsDestroyed() const -> bool {
    std::lock_guard<std::mutex> lock(isDestroyedMutex);
    return isDestroyed;
  }

  auto DestroyImmediately() -> void {
    PrintDebug("Destroying channel immediately.");
    std::lock_guard<std::mutex> lock(isDestroyedMutex);
    isDestroyed = true;

    // Notify all waiting threads
    condition.notify_all();
  }

  ~Channel() override = default;

private:
  std::queue<LuaWrap::Data::LuaType> messages;

  // Mutex for thread safety
  mutable std::mutex mutex;
  mutable std::condition_variable condition;

  mutable std::mutex isDestroyedMutex;
  mutable bool isDestroyed = false;
};

} // namespace Threading