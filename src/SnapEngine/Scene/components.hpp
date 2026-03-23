#include <cstdint>
#include <string>
namespace Engine {

struct Userdata {
  // Index to lua registry for userdata
  int64_t index = 0;
};

struct Name {
  std::string name;
};

} // namespace Engine