#include "SDL3/SDL_events.h"
#include <optional>
namespace Event {
// NOLINTNEXTLINE
extern bool MainLoopRunning;
// NOLINTNEXTLINE
static int32_t ExitCode = 0;

// NOLINTNEXTLINE
auto OnKeyEvent(const SDL_KeyboardEvent &keyEvent) -> void;
auto OnMouseEvent(const SDL_MouseButtonEvent &mouseEvent) -> void;
auto OnMouseMotionEvent(const SDL_MouseMotionEvent &mouseMotionEvent) -> void;
auto OnMouseWheelEvent(const SDL_MouseWheelEvent &mouseWheelEvent) -> void;
auto OnTextEditEvent(const SDL_TextEditingEvent &textEditEvent) -> void;
auto OnTextInputEvent(const SDL_TextInputEvent &textInputEvent) -> void;

auto Pull() -> void;
auto Pop() -> std::optional<SDL_Event>;
} // namespace Event