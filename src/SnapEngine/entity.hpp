#pragma once

#include "Modules/object.hpp"
#include <flecs.h>
namespace Engine {

struct Entity : flecs::entity, Object {};
} // namespace Engine