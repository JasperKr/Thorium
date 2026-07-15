#pragma once

#include <flecs.h>

namespace Engine::Editor {

struct EventBase {
  EventBase() = default;
  EventBase(const EventBase &) = default;
  EventBase(EventBase &&) = default;
  auto operator=(const EventBase &) -> EventBase & = default;
  auto operator=(EventBase &&) -> EventBase & = default;
  virtual ~EventBase() = default;

  virtual void Apply(flecs::entity entity) = 0;
  virtual void Revert(flecs::entity entity) = 0;
};

} // namespace Engine::Editor