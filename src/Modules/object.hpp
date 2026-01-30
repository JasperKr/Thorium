#pragma once

#include "Modules/type.hpp"
#include <atomic>

class Object {
protected:
  Object() = default;

public:
  Object(const Object &) = delete;
  Object(Object &&) = delete;

  auto operator=(const Object &) -> Object & = delete;
  auto operator=(Object &&) -> Object & = delete;

  virtual ~Object() = 0;

  virtual auto ScheduleDestroy() -> void {}
  [[nodiscard]] virtual auto UseDeferredDestruction() const -> bool {
    return false;
  }
  [[nodiscard]] virtual auto GetInstanceType() const -> Type const * = 0;

  void retain() const;
  auto release() -> bool;

  [[nodiscard]] auto getReferenceCount() const -> int;

private:
  mutable std::atomic<int> count{0};
};

template <typename T> class Ref {
public:
  Ref() = default;
  explicit Ref(T *pointer) : ptr(pointer) {
    if (ptr != nullptr) {
      ptr->retain();
    }
  }

  // copy
  Ref(const Ref &reference) : ptr(reference.ptr) {
    if (ptr != nullptr) {
      ptr->retain();
    }
  }
  auto operator=(const Ref &reference) -> Ref & {
    if (this != &reference && ptr != reference.ptr) {
      if (ptr != nullptr) {
        ptr->release();
      }
      ptr = reference.ptr;
      if (ptr != nullptr) {
        ptr->retain();
      }
    }
    return *this;
  }

  // move
  Ref(Ref &&reference) noexcept : ptr(reference.ptr) {
    reference.ptr = nullptr;
  }
  auto operator=(Ref &&reference) noexcept -> Ref & {
    if (this != &reference) {
      if (ptr != nullptr) {
        ptr->release();
      }
      ptr = reference.ptr;
      reference.ptr = nullptr;
    }
    return *this;
  }

  ~Ref() {
    if (ptr != nullptr) {
      ptr->release();
      ptr = nullptr;
    }
  }

  auto operator->() const -> T * { return ptr; }
  auto get() const -> T * { return ptr; }
  auto operator*() const -> T & { return *ptr; }
  [[nodiscard]] auto isValid() const -> bool { return ptr != nullptr; }

  auto reset() -> void {
    if (ptr != nullptr) {
      ptr->release();
      ptr = nullptr;
    }
  }

  template <typename... Args> static auto Make(Args &&...args) -> Ref<T> {
    return Ref<T>(new T(std::forward<Args>(args)...));
  }

  [[nodiscard]] static auto GetType() -> Type const * { return T::GetType(); }
  [[nodiscard]] auto GetInstanceType() const -> Type const * {
    return T::GetType();
  }

private:
  T *ptr = nullptr;
};

struct Proxy {
  const Type *type;
  Object *object;
};