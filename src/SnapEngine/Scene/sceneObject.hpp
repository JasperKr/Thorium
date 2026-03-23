#pragma once

#include "Modules/object.hpp"
#include <variant>

namespace Engine {
using SceneObject =
    std::variant<Ref<struct Node>, Ref<struct Shape>, Ref<struct Model>>;
}