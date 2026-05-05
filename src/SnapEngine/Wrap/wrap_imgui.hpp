#pragma once

#include "Graphics/mesh.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <imgui.h>
#include <lua.h>

namespace Wrap::Imgui {

struct TemporaryCommandList {
  int32_t MaxVertexCount = INT32_MIN;
  int32_t MaxIndexCount = INT32_MIN;
  ImDrawList *DrawList = nullptr;

  Ref<Graphics::Mesh> Mesh;
};

extern std::vector<TemporaryCommandList> TemporaryCommandLists; // NOLINT

auto NewFrame(lua_State *state) -> int;
auto EndFrame(lua_State *state) -> int;
auto Draw(lua_State *state) -> int;

// Imgui event passthrough functions
auto MousePressed(lua_State *state) -> int;
auto MouseReleased(lua_State *state) -> int;
auto MouseMoved(lua_State *state) -> int;
auto MouseWheelMoved(lua_State *state) -> int;

auto KeyPressed(lua_State *state) -> int;
auto KeyReleased(lua_State *state) -> int;
auto TextInput(lua_State *state) -> int;

auto GetImguiContextPtr(lua_State *state) -> int;
auto GetImguiFontAtlasPtr(lua_State *state) -> int;

// Should only be called once, not per-thread.
auto Shutdown(lua_State *state) -> int;

auto wrap_ApplyDefaultStyle(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> ImGuiLib = {
    {"newFrame", NewFrame},
    {"endFrame", EndFrame},
    {"draw", Draw},
    {"mousePressed", MousePressed},
    {"mouseReleased", MouseReleased},
    {"mouseMoved", MouseMoved},
    {"mouseWheelMoved", MouseWheelMoved},
    {"keyPressed", KeyPressed},
    {"keyReleased", KeyReleased},
    {"textInput", TextInput},
    {"getContextPtr", GetImguiContextPtr},
    {"getFontAtlasPtr", GetImguiFontAtlasPtr},
    {"shutdown", Shutdown},
    {"applyDefaultStyle", wrap_ApplyDefaultStyle},
};

const static std::vector<lua_CFunction> childrenInitFunctions = {};

extern "C" inline auto luaopen_gui(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "gui",
      .Functions = ImGuiLib,                          // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT

  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Imgui