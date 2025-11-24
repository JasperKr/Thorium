#include "object.hpp"

Object::Object() : count(1) {}
Object::Object(const Object &other) : count(1) {
  // Copy constructor does not copy reference count
}
Object::~Object() = default;
auto Object::getReferenceCount() const -> int { return count.load(); }
void Object::retain() { count.fetch_add(1); }
void Object::release() {
  if (count.fetch_sub(1) == 1) {
    delete this;
  }
}
