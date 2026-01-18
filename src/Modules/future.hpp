#pragma once

#include "Modules/error.hpp"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <optional>
template <typename T> class Future {
public:
  [[nodiscard]] auto IsReady() const noexcept -> bool {
    return ready.load(std::memory_order_acquire);
  }

  auto DemandData() -> T {
    std::unique_lock<std::mutex> lock(mutex);
    BlockUntilReady();
    ready.store(false, std::memory_order_release);
    return std::move(value);
  }

  auto GetData() -> std::optional<T> {
    std::lock_guard<std::mutex> lock(mutex);
    if (ready.load()) {
      ready.store(false);
      return std::move(value);
    }

    CheckIsReady();

    if (ready.load()) {
      ready.store(false);
      return std::move(value);
    }

    return std::nullopt;
  }

protected:
  // Block thread until ready and calls SetData
  virtual auto BlockUntilReady() -> Error = 0;

  // Checks if ready without blocking, calls SetData if ready
  virtual auto CheckIsReady() -> Error = 0;

  void SetData(T data) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      value = std::move(data);
      ready.store(true, std::memory_order_release);
    }
  }

private:
  T value{};
  std::mutex mutex;
  std::atomic<bool> ready{false};
};
