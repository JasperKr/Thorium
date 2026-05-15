#include "object.hpp"
#include <cassert>
// #define DEBUG_DOUBLE_RELEASE

#ifdef DEBUG_DOUBLE_RELEASE
#include "Modules/console.hpp"
#include "console.hpp"
#include <execinfo.h>
#endif

#ifdef DEBUG_OBJECT_LIFETIMES

std::mutex RefCountsMutex; // NOLINT
std::unordered_map<void const *, std::pair<std::atomic<int>, const char *>>
    RefCounts; // NOLINT
#endif

Object::~Object() {
#ifdef DEBUG_DOUBLE_RELEASE
  if (getReferenceCount() != 0) {
    PrintFatal(
        "Object destructor called on object with non-zero reference count: {}",
        getReferenceCount());

    void *array[100];

    int size = backtrace(array, 100);

    char **strings = backtrace_symbols(array, size);

    for (int i = 0; i < size; i++) {
      PrintInfo("  {}", strings[i]);
    }
  }
#endif
}
auto Object::getReferenceCount() const -> int { return count.load(); }
void Object::retain() const {
  assert(count.load() >= 0);
  count.fetch_add(1);

#ifdef DEBUG_OBJECT_LIFETIMES
  {
    std::lock_guard<std::mutex> lock(RefCountsMutex);
    RefCounts.at(this).first++;
  }
#endif
}
auto Object::release() -> bool {
#ifdef DEBUG_OBJECT_LIFETIMES
  {
    std::lock_guard<std::mutex> lock(RefCountsMutex);
    RefCounts.at(this).first--;
  }
#endif
  auto refcount = count.fetch_sub(1) - 1;
  if (refcount <= 0) {
    if (UseDeferredDestruction()) {
      this->ScheduleDestroy();
    } else {
#ifdef DEBUG_OBJECT_LIFETIMES
      {
        std::lock_guard<std::mutex> lock(RefCountsMutex);
        RefCounts.erase(this);
      }
#endif
#ifndef DEBUG_DOUBLE_RELEASE
      delete this;
#endif
    }

    return true;
  }

#ifdef DEBUG_DOUBLE_RELEASE
  if (refcount < 0) {
    PrintFatal("Object::release() called on object with {} references!",
               refcount);
  }
#endif

  return false;
}
