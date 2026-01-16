#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

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
enum class ThreadStatus : uint8_t { Running, Stopped, Error };
using EncodedLuaData = std::string;

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

  auto Start(const EncodedLuaData &launchArguments, int count) -> void;
  [[nodiscard]] auto GetStatus() const -> ThreadStatus;
  [[nodiscard]] auto GetErrorMessage() const -> std::string;
  auto Stop() -> void;

  auto GetInstanceType() const -> Type const * override { return &threadType; }
  static auto GetType() -> Type const * { return &threadType; }

private:
  static auto Run(Thread *thread, const EncodedLuaData &launchArguments,
                  int count) -> void;

  std::string script;
  lua_State *luaState = nullptr;
  std::thread handle;

  mutable std::mutex statusMutex;
  ThreadStatus status{ThreadStatus::Stopped};
  std::string errorMessage;
};

const static Type channelType = Type("Channel");

struct Channel : Object {
public:
  // Push a message to the channel
  auto Push(const EncodedLuaData &message) -> void;

  // Pop a message from the channel
  auto Pop() -> std::optional<EncodedLuaData>;

  // Peek at the next message without removing it
  [[nodiscard]]
  auto Peek() const -> std::optional<EncodedLuaData>;

  [[nodiscard]]
  auto GetCount() const -> size_t;

  // Demand a message, blocking until one is available
  auto Demand() -> EncodedLuaData;

  auto GetInstanceType() const -> Type const * override { return &channelType; }
  static auto GetType() -> Type const * { return &channelType; }

private:
  std::queue<EncodedLuaData> messages;

  // Mutex for thread safety
  mutable std::mutex mutex;

  mutable std::condition_variable condition;
};

} // namespace Threading