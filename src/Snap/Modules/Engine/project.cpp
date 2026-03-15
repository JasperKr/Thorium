#include "Modules/Engine/project.hpp"
#include "Modules/bindings.hpp"
#include "Modules/error.hpp"
#include "Wrap/wrap.hpp"

#include <algorithm>
#include <imgui.h>
#include <utility>
#include <vector>

#include "lua.hpp"

namespace Engine {

auto Project::LoadBinding(lua_State *state) -> int {
  const std::vector<std::pair<std::string, lua_CFunction>> methods = {
      {"getScene", GetScene},
      {"addScene", AddScene},
      {"removeScene", RemoveScene},
      {"drawUiElement", GetDrawUiElementLuaBinding<Project>()},
  };

  auto binding = Bindings::LuaBoundStruct<Project>("Project");
  binding.RegisterMember<&Project::name>("Name");
  binding.Register(state, methods);

  return 1;
}

auto Project::DrawUiElement() -> Error {
  ImGui::Text("Project: %s", name.c_str());
  ImGui::Text("Objects: %zu", scenes.size());

  return Error::Success();
}

auto Project::GetScene(lua_State *state) -> int {
  auto *project = LuaWrap::ObjectFromLua<Project>(state, 1);
  if (project == nullptr) {
    return luaL_error(state, "Invalid project object");
  }

  if (lua_gettop(state) != 2) {
    return luaL_error(state, "Expected exactly one argument");
  }

  auto index = static_cast<size_t>(luaL_checkinteger(state, 2));
  if (index < 1 || index > project->scenes.size()) {
    return luaL_error(state, "Index out of bounds");
  }

  auto scene = project->scenes[index - 1];
  LuaWrap::PushObject(state, Scene::GetType(), scene.get());
  return 1;
}
auto Project::AddScene(lua_State *state) -> int {
  auto *project = LuaWrap::ObjectFromLua<Project>(state, 1);
  if (project == nullptr) {
    return luaL_error(state, "Invalid project object");
  }

  auto result = LuaWrap::ResultFromLua<Scene>(state, 2);
  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to parse scene object: %s",
                      result.error().message.c_str());
  }

  project->scenes.emplace_back(result.value());

  return 0;
}
auto Project::RemoveScene(lua_State *state) -> int {
  auto *project = LuaWrap::ObjectFromLua<Project>(state, 1);
  if (project == nullptr) {
    return luaL_error(state, "Invalid project object");
  }

  auto result = LuaWrap::ResultFromLua<Scene>(state, 2);
  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to parse scene object: %s",
                      result.error().message.c_str());
  }

  auto iterator = std::ranges::find_if(project->scenes,
                                       [&](const Ref<Scene> &scene) -> bool {
                                         return scene->id == result.value()->id;
                                       });

  if (iterator == project->scenes.end()) {
    return luaL_error(state, "Scene not found in project");
  }

  project->scenes.erase(iterator);

  return 0;
}

} // namespace Engine