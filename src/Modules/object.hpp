#include "Modules/type.hpp"
#include <atomic>

class Object {
public:
  static Type type;

  Object();
  Object(Object &&) = delete;
  auto operator=(const Object &) -> Object & = delete;
  auto operator=(Object &&) -> Object & = delete;
  Object(const Object &other);

  virtual ~Object() = 0;

  auto getReferenceCount() const -> int;
  void retain();
  void release();

private:
  std::atomic<int> count;
};

struct Proxy {
  Type *type;
  Object *object;
};