#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <condition_variable>
#include <cstdint>
#include <queue>
#include <string>
#include <variant>
#include <vector>
#pragma "C"
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

struct Thread {
public:
  auto Start(const std::string &script) -> void;
  [[nodiscard]] auto GetStatus() const -> ThreadStatus;
  auto Stop() -> void;

private:
  lua_State *luaState;
  ThreadStatus status{ThreadStatus::Stopped};
  std::thread handle;
};

struct MessageValue {
  std::variant<double, std::string, bool, Proxy *> data;
};

struct KeyValuePair {
  std::string key;
  MessageValue value;
  std::vector<KeyValuePair> values; // for tables

  [[nodiscard]] auto IsTable() const -> bool { return values.size() > 0; }
};

struct Channel {
public:
  // Push a message to the channel
  auto Push(const KeyValuePair &message) -> void;

  // Pop a message from the channel
  auto Pop() -> Result<KeyValuePair>;

  // Peek at the next message without removing it
  [[nodiscard]]
  auto Peek() -> Result<KeyValuePair>;

  // Demand a message, blocking until one is available
  auto Demand() -> KeyValuePair;

private:
  std::queue<KeyValuePair> messages;

  // Mutex for thread safety
  mutable std::mutex mutex;

  mutable std::condition_variable condition;
};

} // namespace Threading