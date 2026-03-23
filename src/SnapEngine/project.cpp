#include "project.hpp"
#include "Modules/bindings.hpp"
#include "Modules/error.hpp"
#include "Scene/model.hpp"
#include "Scene/node.hpp"
#include "Scene/shape.hpp"

#include <imgui.h>
#include <utility>
#include <vector>

#include "lua.hpp"

namespace Engine {

auto Project::LoadBinding(lua_State *state) -> int {
  const std::vector<std::pair<std::string, lua_CFunction>> methods = {
      {"drawUiElement", GetDrawUiElementLuaBinding<Project>()},
  };

  auto binding = Bindings::LuaBoundStruct<Project>("Project");
  binding.RegisterMember<&Project::name>("Name");
  binding.RegisterStandardVectorMember<&Project::scenes>(
      "Scenes", "The scenes in the project.");
  binding.Register(state, methods);

  return 1;
}

auto Project::DrawUiElement() -> Error {
  ImGui::Text("Project: %s", name.c_str());
  ImGui::Text("Objects: %zu", scenes.size());

  return Error::Success();
}

} // namespace Engine