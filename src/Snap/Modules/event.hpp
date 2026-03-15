#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Event {
// NOLINTNEXTLINE
extern bool MainLoopRunning;
// NOLINTNEXTLINE
static int32_t ExitCode = 0;

using EventValue =
    std::variant<int32_t, uint32_t, float, double, std::string, bool>;

struct Event {
  std::string Name;
  std::vector<EventValue> Values;
};

auto Pull() -> void;
auto Pop() -> std::optional<Event>;
auto Push(const Event &event) -> void;
auto Quit() -> void;
} // namespace Event