#pragma once

#include "Scene/scene.hpp"
#include <flecs.h>
namespace Engine::Editor {

enum class MatchType : uint8_t {
  This,
  Child,
  None,
};

auto DrawEntity(const flecs::entity &entity) -> void;
auto DrawEntityHierarchy(const flecs::entity &entity, std::string_view filter)
    -> void;
auto DrawSceneHierarchy(const Engine::Scene &scene) -> Error;

} // namespace Engine::Editor