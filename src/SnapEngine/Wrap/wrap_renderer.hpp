#pragma once

#include "Scene/model.hpp"
#include "Scene/node.hpp"
#include "Scene/shape.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include "material.hpp"
#include <vector>

namespace Engine::Renderer {

auto wrap_NewMaterial(lua_State *state) -> int;

static const std::vector<luaL_Reg> RendererLib = {
    {"newMaterial", wrap_NewMaterial},
};

static const std::vector<lua_CFunction> childrenInitFunctions{
    Material::LoadBinding,
    Node::LoadBinding,
    Model::LoadBinding,
    Shape::LoadBinding,
};

extern "C" inline auto luaopen_renderer(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "renderer",
      .Functions = RendererLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,
  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Engine::Renderer