#include "object.hpp"

Object::~Object() = default;
auto Object::getReferenceCount() const -> int { return count.load(); }
void Object::retain() const { count.fetch_add(1); }
void Object::release() {
  if (count.fetch_sub(1) == 1) {
    if (UseDeferredDestruction()) {
      this->ScheduleDestroy();
    } else {
      delete this;
    }
  }
}
