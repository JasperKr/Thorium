#include "object.hpp"
#include <cassert>
// #define DEBUG_DOUBLE_RELEASE

#ifdef DEBUG_DOUBLE_RELEASE
#include "Modules/console.hpp"
#include "console.hpp"
#include <execinfo.h>
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
}
auto Object::release() -> bool {
  auto refcount = count.fetch_sub(1) - 1;
  if (refcount <= 0) {
    if (UseDeferredDestruction()) {
      this->ScheduleDestroy();
    } else {
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
