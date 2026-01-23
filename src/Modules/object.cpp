#include "object.hpp"
#include "console.hpp"
// #define DEBUG_DOUBLE_RELEASE

#ifdef DEBUG_DOUBLE_RELEASE
#include "Modules/console.hpp"
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
void Object::retain() const { count.fetch_add(1); }
auto Object::release() -> bool {
  auto fetchedCount = count.fetch_sub(1);
  if (fetchedCount == 1) {
#ifndef DEBUG_DOUBLE_RELEASE
    if (UseDeferredDestruction()) {
      this->ScheduleDestroy();
    } else {
      delete this;
    }
#endif

    return true;
  }

#ifdef DEBUG_DOUBLE_RELEASE
  if (fetchedCount < 1) {
    PrintFatal("Object::release() called on object with zero references!");
  }
#endif

  return false;
}
