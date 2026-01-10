#include "wrap_imgui.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/rendertarget.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/vertexformat.hpp"
#include "Modules/Editor/gui.hpp"
#include "Modules/Peripherals/keyboard.hpp"
#include "Modules/Peripherals/mouse.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/imagedata.hpp"
#include "Modules/object.hpp"
#include "Wrap/wrap.hpp"
#include "imgui.h"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <unordered_map>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

namespace Wrap::Imgui {

auto Begin(lua_State *state) -> int {
  const auto *name = luaL_checkstring(state, 1);
  auto flags = static_cast<ImGuiWindowFlags>(luaL_optinteger(state, 2, 0));

  auto result = ImGui::Begin(name, nullptr, flags);

  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto End(lua_State *state) -> int {
  ImGui::End();
  return 0;
}

auto BeginChild(lua_State *state) -> int {
  const auto *identifier = luaL_checkstring(state, 1);
  auto width = static_cast<float>(luaL_checknumber(state, 2));
  auto height = static_cast<float>(luaL_checknumber(state, 3));
  auto border = static_cast<ImGuiChildFlags>(luaL_checkinteger(state, 4));
  auto flags = static_cast<ImGuiWindowFlags>(luaL_optinteger(state, 5, 0));

  auto result =
      ImGui::BeginChild(identifier, ImVec2{width, height}, border, flags);

  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto EndChild(lua_State *state) -> int {
  ImGui::EndChild();
  return 0;
}

auto BeginGroup(lua_State *state) -> int {
  ImGui::BeginGroup();
  return 0;
}
auto EndGroup(lua_State *state) -> int {
  ImGui::EndGroup();
  return 0;
}

auto Separator(lua_State *state) -> int {
  ImGui::Separator();
  return 0;
}
auto SeparatorText(lua_State *state) -> int {
  const auto *text = luaL_checkstring(state, 1);
  ImGui::SeparatorText(text);
  return 0;
}

auto Dummy(lua_State *state) -> int {
  auto width = static_cast<float>(luaL_checknumber(state, 1));
  auto height = static_cast<float>(luaL_checknumber(state, 2));
  ImGui::Dummy(ImVec2{width, height});
  return 0;
}
auto Spacing(lua_State *state) -> int {
  ImGui::Spacing();
  return 0;
}
auto NewLine(lua_State *state) -> int {
  ImGui::NewLine();
  return 0;
}
auto Indent(lua_State *state) -> int {
  auto amount = luaL_optinteger(state, 1, 0);
  ImGui::Indent(static_cast<float>(amount));
  return 0;
}
auto Unindent(lua_State *state) -> int {
  auto amount = luaL_optinteger(state, 1, 0);
  ImGui::Unindent(static_cast<float>(amount));
  return 0;
}
auto SameLine(lua_State *state) -> int {
  auto offsetFromStartX = static_cast<float>(luaL_optnumber(state, 1, 0.0));
  auto spacingW = static_cast<float>(luaL_optnumber(state, 2, -1.0));
  ImGui::SameLine(offsetFromStartX, spacingW);
  return 0;
}

auto GetCursorPos(lua_State *state) -> int {
  auto pos = ImGui::GetCursorPos();
  lua_pushnumber(state, pos.x);
  lua_pushnumber(state, pos.y);
  return 2;
}
auto SetCursorPos(lua_State *state) -> int {
  auto x_position = static_cast<float>(luaL_checknumber(state, 1));
  auto y_position = static_cast<float>(luaL_checknumber(state, 2));
  ImGui::SetCursorPos(ImVec2{x_position, y_position});
  return 0;
}
auto SetCursorPosX(lua_State *state) -> int {
  auto x_position = static_cast<float>(luaL_checknumber(state, 1));
  ImGui::SetCursorPosX(x_position);
  return 0;
}
auto SetCursorPosY(lua_State *state) -> int {
  auto y_position = static_cast<float>(luaL_checknumber(state, 1));
  ImGui::SetCursorPosY(y_position);
  return 0;
}

auto GetCursorScreenPos(lua_State *state) -> int {
  auto pos = ImGui::GetCursorScreenPos();
  lua_pushnumber(state, pos.x);
  lua_pushnumber(state, pos.y);
  return 2;
}
auto SetCursorScreenPos(lua_State *state) -> int {
  auto x_position = static_cast<float>(luaL_checknumber(state, 1));
  auto y_position = static_cast<float>(luaL_checknumber(state, 2));
  ImGui::SetCursorScreenPos(ImVec2{x_position, y_position});
  return 0;
}
auto GetCursorStartPos(lua_State *state) -> int {
  auto pos = ImGui::GetCursorStartPos();
  lua_pushnumber(state, pos.x);
  lua_pushnumber(state, pos.y);
  return 2;
}

auto GetContentRegionAvail(lua_State *state) -> int {
  auto region = ImGui::GetContentRegionAvail();
  lua_pushnumber(state, region.x);
  lua_pushnumber(state, region.y);
  return 2;
}
auto GetContentRegionMax(lua_State *state) -> int {
  auto region = ImGui::GetContentRegionMax();
  lua_pushnumber(state, region.x);
  lua_pushnumber(state, region.y);
  return 2;
}

auto GetWindowContentRegionMin(lua_State *state) -> int {
  auto region = ImGui::GetWindowContentRegionMin();
  lua_pushnumber(state, region.x);
  lua_pushnumber(state, region.y);
  return 2;
}
auto GetWindowContentRegionMax(lua_State *state) -> int {
  auto region = ImGui::GetWindowContentRegionMax();
  lua_pushnumber(state, region.x);
  lua_pushnumber(state, region.y);
  return 2;
}

auto GetWindowPos(lua_State *state) -> int {
  auto pos = ImGui::GetWindowPos();
  lua_pushnumber(state, pos.x);
  lua_pushnumber(state, pos.y);
  return 2;
}
auto GetWindowSize(lua_State *state) -> int {
  auto size = ImGui::GetWindowSize();
  lua_pushnumber(state, size.x);
  lua_pushnumber(state, size.y);
  return 2;
}
auto SetWindowPos(lua_State *state) -> int {
  auto x_position = static_cast<float>(luaL_checknumber(state, 1));
  auto y_position = static_cast<float>(luaL_checknumber(state, 2));
  ImGui::SetWindowPos(ImVec2{x_position, y_position});
  return 0;
}
auto SetWindowSize(lua_State *state) -> int {
  auto width = static_cast<float>(luaL_checknumber(state, 1));
  auto height = static_cast<float>(luaL_checknumber(state, 2));
  ImGui::SetWindowSize(ImVec2{width, height});
  return 0;
}
auto SetWindowCollapsed(lua_State *state) -> int {
  auto collapsed = static_cast<bool>(lua_toboolean(state, 1));
  ImGui::SetWindowCollapsed(collapsed);
  return 0;
}
auto SetWindowFocus(lua_State *state) -> int {
  ImGui::SetWindowFocus();
  return 0;
}

auto IsWindowFocused(lua_State *state) -> int {
  auto focused = ImGui::IsWindowFocused();
  lua_pushboolean(state, static_cast<int>(focused));
  return 1;
}
auto IsWindowCollapsed(lua_State *state) -> int {
  auto collapsed = ImGui::IsWindowCollapsed();
  lua_pushboolean(state, static_cast<int>(collapsed));
  return 1;
}

auto GetWindowWidth(lua_State *state) -> int {
  auto size = ImGui::GetWindowSize();
  lua_pushnumber(state, size.x);
  return 1;
}
auto GetWindowHeight(lua_State *state) -> int {
  auto size = ImGui::GetWindowSize();
  lua_pushnumber(state, size.y);
  return 1;
}
auto GetWindowDrawList(lua_State *state) -> int {
  // auto *drawList = ImGui::GetWindowDrawList();
  // auto **userdata =
  //     static_cast<ImDrawList **>(lua_newuserdata(state, sizeof(ImDrawList *)));
  // *userdata = drawList;
  // luaL_getmetatable(state, "ImGuiDrawList");
  // lua_setmetatable(state, -2);
  return 1;
}

auto GetScrollX(lua_State *state) -> int {
  auto scrollX = ImGui::GetScrollX();
  lua_pushnumber(state, scrollX);
  return 1;
}
auto GetScrollY(lua_State *state) -> int {
  auto scrollY = ImGui::GetScrollY();
  lua_pushnumber(state, scrollY);
  return 1;
}
auto SetScrollX(lua_State *state) -> int {
  auto scrollX = static_cast<float>(luaL_checknumber(state, 1));
  ImGui::SetScrollX(scrollX);
  return 0;
}
auto SetScrollY(lua_State *state) -> int {
  auto scrollY = static_cast<float>(luaL_checknumber(state, 1));
  ImGui::SetScrollY(scrollY);
  return 0;
}

auto Text(lua_State *state) -> int {
  const auto *text = luaL_checkstring(state, 1);
  ImGui::Text("%s", text);
  return 0;
}
auto TextColored(lua_State *state) -> int {
  auto color_r = static_cast<float>(luaL_checknumber(state, 1));
  auto color_g = static_cast<float>(luaL_checknumber(state, 2));
  auto color_b = static_cast<float>(luaL_checknumber(state, 3));
  auto color_a = static_cast<float>(luaL_checknumber(state, 4));
  const auto *text = luaL_checkstring(state, 5);
  ImGui::TextColored(ImVec4{color_r, color_g, color_b, color_a}, "%s", text);
  return 0;
}
auto TextDisabled(lua_State *state) -> int {
  const auto *text = luaL_checkstring(state, 1);
  ImGui::TextDisabled("%s", text);
  return 0;
}
auto TextWrapped(lua_State *state) -> int {
  const auto *text = luaL_checkstring(state, 1);
  ImGui::TextWrapped("%s", text);
  return 0;
}
auto LabelText(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  const auto *text = luaL_checkstring(state, 2);
  ImGui::LabelText(label, "%s", text);
  return 0;
}
auto BulletText(lua_State *state) -> int {
  const auto *text = luaL_checkstring(state, 1);
  ImGui::BulletText("%s", text);
  return 0;
}
auto Bullet(lua_State *state) -> int {
  ImGui::Bullet();
  return 0;
}

auto Button(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto width = static_cast<float>(luaL_optnumber(state, 2, 0.0));
  auto height = static_cast<float>(luaL_optnumber(state, 3, 0.0));
  auto result = ImGui::Button(label, ImVec2{width, height});
  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto SmallButton(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto result = ImGui::SmallButton(label);
  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto InvisibleButton(lua_State *state) -> int {
  const auto *str_id = luaL_checkstring(state, 1);
  auto width = static_cast<float>(luaL_checknumber(state, 2));
  auto height = static_cast<float>(luaL_checknumber(state, 3));
  auto result = ImGui::InvisibleButton(str_id, ImVec2{width, height});
  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto ArrowButton(lua_State *state) -> int {
  const auto *str_id = luaL_checkstring(state, 1);
  auto dir = static_cast<ImGuiDir>(luaL_checkinteger(state, 2));
  auto result = ImGui::ArrowButton(str_id, dir);
  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto ImageButton(lua_State *state) -> int {
  const auto *name = luaL_checkstring(state, 1);
  auto *texture = LuaWrap::ObjectFromLua<Graphics::Texture::Texture>(state, 2);
  auto width = static_cast<float>(luaL_checknumber(state, 3));
  auto height = static_cast<float>(luaL_checknumber(state, 4));
  auto uv0_x = static_cast<float>(luaL_optnumber(state, 5, 0.0));
  auto uv0_y = static_cast<float>(luaL_optnumber(state, 6, 0.0));
  auto uv1_x = static_cast<float>(luaL_optnumber(state, 7, 1.0));
  auto uv1_y = static_cast<float>(luaL_optnumber(state, 8, 1.0));
  auto bg_col_r = static_cast<float>(luaL_optnumber(state, 9, 0.0));
  auto bg_col_g = static_cast<float>(luaL_optnumber(state, 10, 0.0));
  auto bg_col_b = static_cast<float>(luaL_optnumber(state, 11, 0.0));
  auto bg_col_a = static_cast<float>(luaL_optnumber(state, 12, 0.0));
  auto tint_col_r = static_cast<float>(luaL_optnumber(state, 13, 1.0));
  auto tint_col_g = static_cast<float>(luaL_optnumber(state, 14, 1.0));
  auto tint_col_b = static_cast<float>(luaL_optnumber(state, 15, 1.0));
  auto tint_col_a = static_cast<float>(luaL_optnumber(state, 16, 1.0));
  auto result = ImGui::ImageButton(
      name, reinterpret_cast<ImTextureID>(texture), // NOLINT
      ImVec2{width, height}, ImVec2{uv0_x, uv0_y}, ImVec2{uv1_x, uv1_y},
      ImVec4{bg_col_r, bg_col_g, bg_col_b, bg_col_a},
      ImVec4{tint_col_r, tint_col_g, tint_col_b, tint_col_a});
  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto Image(lua_State *state) -> int {
  auto *texture = LuaWrap::ObjectFromLua<Graphics::Texture::Texture>(state, 1);
  auto width = static_cast<float>(luaL_checknumber(state, 2));
  auto height = static_cast<float>(luaL_checknumber(state, 3));
  auto uv0_x = static_cast<float>(luaL_optnumber(state, 4, 0.0));
  auto uv0_y = static_cast<float>(luaL_optnumber(state, 5, 0.0));
  auto uv1_x = static_cast<float>(luaL_optnumber(state, 6, 1.0));
  auto uv1_y = static_cast<float>(luaL_optnumber(state, 7, 1.0));
  ImGui::Image(reinterpret_cast<ImTextureID>(texture), // NOLINT
               ImVec2{width, height}, ImVec2{uv0_x, uv0_y},
               ImVec2{uv1_x, uv1_y});
  return 0;
}
auto Checkbox(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value = static_cast<bool>(lua_toboolean(state, 2));
  auto result = ImGui::Checkbox(label, &value);
  lua_pushboolean(state, static_cast<int>(result));
  lua_pushboolean(state, static_cast<int>(value));
  return 2;
}
auto RadioButton(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto active = static_cast<bool>(lua_toboolean(state, 2));
  auto result = ImGui::RadioButton(label, active);
  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto ProgressBar(lua_State *state) -> int {
  auto fraction = static_cast<float>(luaL_checknumber(state, 1));
  auto width = static_cast<float>(luaL_optnumber(state, 2, -1.0));
  auto height = static_cast<float>(luaL_optnumber(state, 3, 0.0));
  const auto *overlay = luaL_optstring(state, 4, nullptr);
  ImGui::ProgressBar(fraction, ImVec2{width, height},
                     overlay != nullptr ? overlay : nullptr);
  return 0;
}

auto SliderFloat(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value = static_cast<float>(luaL_checknumber(state, 2));
  auto min = static_cast<float>(luaL_checknumber(state, 3));
  auto max = static_cast<float>(luaL_checknumber(state, 4));
  const auto *format = luaL_optstring(state, 5, "%.3f");
  auto flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(state, 6, 0));

  auto result = ImGui::SliderFloat(label, &value, min, max, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, value);
  return 2;
}
auto SliderInt(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value = static_cast<int>(luaL_checkinteger(state, 2));
  auto min = static_cast<int>(luaL_checkinteger(state, 3));
  auto max = static_cast<int>(luaL_checkinteger(state, 4));
  const auto *format = luaL_optstring(state, 5, "%d");
  auto flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(state, 6, 0));

  auto result = ImGui::SliderInt(label, &value, min, max, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushinteger(state, value);
  return 2;
}
auto VSliderFloat(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto width = static_cast<float>(luaL_checknumber(state, 2));
  auto height = static_cast<float>(luaL_checknumber(state, 3));
  auto value = static_cast<float>(luaL_checknumber(state, 4));
  auto min = static_cast<float>(luaL_checknumber(state, 5));
  auto max = static_cast<float>(luaL_checknumber(state, 6));
  const auto *format = luaL_optstring(state, 7, "%.3f");
  auto flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(state, 8, 0));

  auto result = ImGui::VSliderFloat(label, ImVec2{width, height}, &value, min,
                                    max, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, value);
  return 2;
}
auto VSliderInt(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto width = static_cast<float>(luaL_checknumber(state, 2));
  auto height = static_cast<float>(luaL_checknumber(state, 3));
  auto value = static_cast<int>(luaL_checkinteger(state, 4));
  auto min = static_cast<int>(luaL_checkinteger(state, 5));
  auto max = static_cast<int>(luaL_checkinteger(state, 6));
  const auto *format = luaL_optstring(state, 7, "%d");
  auto flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(state, 8, 0));

  auto result = ImGui::VSliderInt(label, ImVec2{width, height}, &value, min,
                                  max, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushinteger(state, value);
  return 2;
}

auto DragFloat(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value = static_cast<float>(luaL_checknumber(state, 2));
  auto speed = static_cast<float>(luaL_optnumber(state, 3, 1.0F));
  auto min = static_cast<float>(luaL_optnumber(state, 4, 0.0F));
  auto max = static_cast<float>(luaL_optnumber(state, 5, 0.0F));
  const auto *format = luaL_optstring(state, 6, "%.3f");
  auto flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(state, 7, 0));

  auto result = ImGui::DragFloat(label, &value, speed, min, max, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, value);
  return 2;
}
auto DragInt(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value = static_cast<int>(luaL_checkinteger(state, 2));
  auto speed = static_cast<float>(luaL_optnumber(state, 3, 1.0F));
  auto min = static_cast<int>(luaL_optnumber(state, 4, 0));
  auto max = static_cast<int>(luaL_optnumber(state, 5, 0));
  const auto *format = luaL_optstring(state, 6, "%d");
  auto flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(state, 7, 0));

  auto result = ImGui::DragInt(label, &value, speed, min, max, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushinteger(state, value);
  return 2;
}
auto DragFloat2(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value_x = static_cast<float>(luaL_checknumber(state, 2));
  auto value_y = static_cast<float>(luaL_checknumber(state, 3));
  auto speed = static_cast<float>(luaL_optnumber(state, 4, 1.0F));
  auto min = static_cast<float>(luaL_optnumber(state, 5, 0.0F));
  auto max = static_cast<float>(luaL_optnumber(state, 6, 0.0F));
  const auto *format = luaL_optstring(state, 7, "%.3f");
  auto flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(state, 8, 0));

  std::array<float, 2> value{value_x, value_y};
  auto result =
      ImGui::DragFloat2(label, value.data(), speed, min, max, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, value[0]);
  lua_pushnumber(state, value[1]);
  return 3;
}
auto DragInt2(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value_x = static_cast<int>(luaL_checkinteger(state, 2));
  auto value_y = static_cast<int>(luaL_checkinteger(state, 3));
  auto speed = static_cast<float>(luaL_optnumber(state, 4, 1.0F));
  auto min = static_cast<int>(luaL_optnumber(state, 5, 0));
  auto max = static_cast<int>(luaL_optnumber(state, 6, 0));
  const auto *format = luaL_optstring(state, 7, "%d");
  auto flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(state, 8, 0));

  std::array<int, 2> value{value_x, value_y};
  auto result =
      ImGui::DragInt2(label, value.data(), speed, min, max, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushinteger(state, value[0]);
  lua_pushinteger(state, value[1]);
  return 3;
}
auto DragFloat3(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value_x = static_cast<float>(luaL_checknumber(state, 2));
  auto value_y = static_cast<float>(luaL_checknumber(state, 3));
  auto value_z = static_cast<float>(luaL_checknumber(state, 4));
  auto speed = static_cast<float>(luaL_optnumber(state, 5, 1.0F));
  auto min = static_cast<float>(luaL_optnumber(state, 6, 0.0F));
  auto max = static_cast<float>(luaL_optnumber(state, 7, 0.0F));
  const auto *format = luaL_optstring(state, 8, "%.3f");
  auto flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(state, 9, 0));

  std::array<float, 3> value{value_x, value_y, value_z};
  auto result =
      ImGui::DragFloat3(label, value.data(), speed, min, max, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, value[0]);
  lua_pushnumber(state, value[1]);
  lua_pushnumber(state, value[2]);
  return 4;
}
auto DragInt3(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value_x = static_cast<int>(luaL_checkinteger(state, 2));
  auto value_y = static_cast<int>(luaL_checkinteger(state, 3));
  auto value_z = static_cast<int>(luaL_checkinteger(state, 4));
  auto speed = static_cast<float>(luaL_optnumber(state, 5, 1.0F));
  auto min = static_cast<int>(luaL_optnumber(state, 6, 0));
  auto max = static_cast<int>(luaL_optnumber(state, 7, 0));
  const auto *format = luaL_optstring(state, 8, "%d");
  auto flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(state, 9, 0));

  std::array<int, 3> value{value_x, value_y, value_z};
  auto result =
      ImGui::DragInt3(label, value.data(), speed, min, max, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushinteger(state, value[0]);
  lua_pushinteger(state, value[1]);
  lua_pushinteger(state, value[2]);
  return 4;
}
auto DragFloat4(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value_x = static_cast<float>(luaL_checknumber(state, 2));
  auto value_y = static_cast<float>(luaL_checknumber(state, 3));
  auto value_z = static_cast<float>(luaL_checknumber(state, 4));
  auto value_w = static_cast<float>(luaL_checknumber(state, 5));
  auto speed = static_cast<float>(luaL_optnumber(state, 6, 1.0F));
  auto min = static_cast<float>(luaL_optnumber(state, 7, 0.0F));
  auto max = static_cast<float>(luaL_optnumber(state, 8, 0.0F));
  const auto *format = luaL_optstring(state, 9, "%.3f");
  auto flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(state, 10, 0));

  std::array<float, 4> value{value_x, value_y, value_z, value_w};
  auto result =
      ImGui::DragFloat4(label, value.data(), speed, min, max, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, value[0]);
  lua_pushnumber(state, value[1]);
  lua_pushnumber(state, value[2]);
  lua_pushnumber(state, value[3]);
  return 5;
}
auto DragInt4(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value_x = static_cast<int>(luaL_checkinteger(state, 2));
  auto value_y = static_cast<int>(luaL_checkinteger(state, 3));
  auto value_z = static_cast<int>(luaL_checkinteger(state, 4));
  auto value_w = static_cast<int>(luaL_checkinteger(state, 5));
  auto speed = static_cast<float>(luaL_optnumber(state, 6, 1.0F));
  auto min = static_cast<int>(luaL_optnumber(state, 7, 0));
  auto max = static_cast<int>(luaL_optnumber(state, 8, 0));
  const auto *format = luaL_optstring(state, 9, "%d");
  auto flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(state, 10, 0));

  std::array<int, 4> value{value_x, value_y, value_z, value_w};
  auto result =
      ImGui::DragInt4(label, value.data(), speed, min, max, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushinteger(state, value[0]);
  lua_pushinteger(state, value[1]);
  lua_pushinteger(state, value[2]);
  lua_pushinteger(state, value[3]);
  return 5;
}

auto InputText(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto buffer = std::string(luaL_checkstring(state, 2));
  auto buffer_size = static_cast<size_t>(luaL_checkinteger(state, 3));
  auto flags = static_cast<ImGuiInputTextFlags>(luaL_optinteger(state, 4, 0));

  buffer.resize(buffer_size);

  auto result = ImGui::InputText(label, buffer.data(),
                                 static_cast<int>(buffer.size()), flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushstring(state, buffer.c_str());
  return 2;
}
auto InputTextMultiline(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto buffer = std::string(luaL_checkstring(state, 2));
  auto width = static_cast<float>(luaL_checknumber(state, 3));
  auto height = static_cast<float>(luaL_checknumber(state, 4));
  auto buffer_size = static_cast<size_t>(luaL_checkinteger(state, 5));
  auto flags = static_cast<ImGuiInputTextFlags>(luaL_optinteger(state, 6, 0));

  buffer.resize(buffer_size);

  auto result = ImGui::InputTextMultiline(label, buffer.data(),
                                          static_cast<int>(buffer.size()),
                                          ImVec2{width, height}, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushstring(state, buffer.c_str());
  return 2;
}
auto InputFloat(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value = static_cast<float>(luaL_checknumber(state, 2));
  auto step = static_cast<float>(luaL_optnumber(state, 3, 0.0F));
  auto step_fast = static_cast<float>(luaL_optnumber(state, 4, 0.0F));
  const auto *format = luaL_optstring(state, 5, "%.3f");
  auto flags = static_cast<ImGuiInputTextFlags>(luaL_optinteger(state, 6, 0));

  auto result =
      ImGui::InputFloat(label, &value, step, step_fast, format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, value);
  return 2;
}
auto InputInt(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value = static_cast<int>(luaL_checkinteger(state, 2));
  auto step = static_cast<int>(luaL_optnumber(state, 3, 0));
  auto step_fast = static_cast<int>(luaL_optnumber(state, 4, 0));
  auto flags = static_cast<ImGuiInputTextFlags>(luaL_optinteger(state, 5, 0));

  auto result = ImGui::InputInt(label, &value, step, step_fast, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushinteger(state, value);
  return 2;
}
auto InputFloat2(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value_x = static_cast<float>(luaL_checknumber(state, 2));
  auto value_y = static_cast<float>(luaL_checknumber(state, 3));
  const auto *format = luaL_optstring(state, 4, "%.3f");
  auto flags = static_cast<ImGuiInputTextFlags>(luaL_optinteger(state, 5, 0));

  std::array<float, 2> value{value_x, value_y};
  auto result = ImGui::InputFloat2(label, value.data(), format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, value[0]);
  lua_pushnumber(state, value[1]);
  return 3;
}
auto InputInt2(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value_x = static_cast<int>(luaL_checkinteger(state, 2));
  auto value_y = static_cast<int>(luaL_checkinteger(state, 3));
  auto flags = static_cast<ImGuiInputTextFlags>(luaL_optinteger(state, 4, 0));

  std::array<int, 2> value{value_x, value_y};
  auto result = ImGui::InputInt2(label, value.data(), flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushinteger(state, value[0]);
  lua_pushinteger(state, value[1]);
  return 3;
}
auto InputFloat3(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value_x = static_cast<float>(luaL_checknumber(state, 2));
  auto value_y = static_cast<float>(luaL_checknumber(state, 3));
  auto value_z = static_cast<float>(luaL_checknumber(state, 4));
  const auto *format = luaL_optstring(state, 5, "%.3f");
  auto flags = static_cast<ImGuiInputTextFlags>(luaL_optinteger(state, 6, 0));

  std::array<float, 3> value{value_x, value_y, value_z};
  auto result = ImGui::InputFloat3(label, value.data(), format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, value[0]);
  lua_pushnumber(state, value[1]);
  lua_pushnumber(state, value[2]);
  return 4;
}
auto InputInt3(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value_x = static_cast<int>(luaL_checkinteger(state, 2));
  auto value_y = static_cast<int>(luaL_checkinteger(state, 3));
  auto value_z = static_cast<int>(luaL_checkinteger(state, 4));
  auto flags = static_cast<ImGuiInputTextFlags>(luaL_optinteger(state, 5, 0));

  std::array<int, 3> value{value_x, value_y, value_z};
  auto result = ImGui::InputInt3(label, value.data(), flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushinteger(state, value[0]);
  lua_pushinteger(state, value[1]);
  lua_pushinteger(state, value[2]);
  return 4;
}
auto InputFloat4(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value_x = static_cast<float>(luaL_checknumber(state, 2));
  auto value_y = static_cast<float>(luaL_checknumber(state, 3));
  auto value_z = static_cast<float>(luaL_checknumber(state, 4));
  auto value_w = static_cast<float>(luaL_checknumber(state, 5));
  const auto *format = luaL_optstring(state, 6, "%.3f");
  auto flags = static_cast<ImGuiInputTextFlags>(luaL_optinteger(state, 7, 0));

  std::array<float, 4> value{value_x, value_y, value_z, value_w};
  auto result = ImGui::InputFloat4(label, value.data(), format, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, value[0]);
  lua_pushnumber(state, value[1]);
  lua_pushnumber(state, value[2]);
  lua_pushnumber(state, value[3]);
  return 5;
}
auto InputInt4(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto value_x = static_cast<int>(luaL_checkinteger(state, 2));
  auto value_y = static_cast<int>(luaL_checkinteger(state, 3));
  auto value_z = static_cast<int>(luaL_checkinteger(state, 4));
  auto value_w = static_cast<int>(luaL_checkinteger(state, 5));
  auto flags = static_cast<ImGuiInputTextFlags>(luaL_optinteger(state, 6, 0));

  std::array<int, 4> value{value_x, value_y, value_z, value_w};
  auto result = ImGui::InputInt4(label, value.data(), flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushinteger(state, value[0]);
  lua_pushinteger(state, value[1]);
  lua_pushinteger(state, value[2]);
  lua_pushinteger(state, value[3]);
  return 5;
}

auto ColorEdit3(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto color_r = static_cast<float>(luaL_checknumber(state, 2));
  auto color_g = static_cast<float>(luaL_checknumber(state, 3));
  auto color_b = static_cast<float>(luaL_checknumber(state, 4));
  auto flags = static_cast<ImGuiColorEditFlags>(luaL_optinteger(state, 5, 0));

  std::array<float, 3> color{color_r, color_g, color_b};
  auto result = ImGui::ColorEdit3(label, color.data(), flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, color[0]);
  lua_pushnumber(state, color[1]);
  lua_pushnumber(state, color[2]);
  return 4;
}
auto ColorEdit4(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto color_r = static_cast<float>(luaL_checknumber(state, 2));
  auto color_g = static_cast<float>(luaL_checknumber(state, 3));
  auto color_b = static_cast<float>(luaL_checknumber(state, 4));
  auto color_a = static_cast<float>(luaL_checknumber(state, 5));
  auto flags = static_cast<ImGuiColorEditFlags>(luaL_optinteger(state, 6, 0));

  std::array<float, 4> color{color_r, color_g, color_b, color_a};
  auto result = ImGui::ColorEdit4(label, color.data(), flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, color[0]);
  lua_pushnumber(state, color[1]);
  lua_pushnumber(state, color[2]);
  lua_pushnumber(state, color[3]);
  return 5;
}
auto ColorPicker3(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto color_r = static_cast<float>(luaL_checknumber(state, 2));
  auto color_g = static_cast<float>(luaL_checknumber(state, 3));
  auto color_b = static_cast<float>(luaL_checknumber(state, 4));
  auto flags = static_cast<ImGuiColorEditFlags>(luaL_optinteger(state, 5, 0));

  std::array<float, 3> color{color_r, color_g, color_b};
  auto result = ImGui::ColorPicker3(label, color.data(), flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, color[0]);
  lua_pushnumber(state, color[1]);
  lua_pushnumber(state, color[2]);
  return 4;
}
auto ColorPicker4(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto color_r = static_cast<float>(luaL_checknumber(state, 2));
  auto color_g = static_cast<float>(luaL_checknumber(state, 3));
  auto color_b = static_cast<float>(luaL_checknumber(state, 4));
  auto color_a = static_cast<float>(luaL_checknumber(state, 5));
  auto flags = static_cast<ImGuiColorEditFlags>(luaL_optinteger(state, 6, 0));

  std::array<float, 4> color{color_r, color_g, color_b, color_a};
  auto result = ImGui::ColorPicker4(label, color.data(), flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushnumber(state, color[0]);
  lua_pushnumber(state, color[1]);
  lua_pushnumber(state, color[2]);
  lua_pushnumber(state, color[3]);
  return 5;
}

auto BeginCombo(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  const auto *preview_value = luaL_checkstring(state, 2);
  auto flags = static_cast<ImGuiComboFlags>(luaL_optinteger(state, 3, 0));

  auto result = ImGui::BeginCombo(label, preview_value, flags);

  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto EndCombo(lua_State *state) -> int {
  ImGui::EndCombo();
  return 0;
}

auto BeginTabBar(lua_State *state) -> int {
  const auto *str_id = luaL_checkstring(state, 1);
  auto flags = static_cast<ImGuiTabBarFlags>(luaL_optinteger(state, 2, 0));

  auto result = ImGui::BeginTabBar(str_id, flags);

  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto EndTabBar(lua_State *state) -> int {
  ImGui::EndTabBar();
  return 0;
}
auto BeginTabItem(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto open = static_cast<bool>(lua_toboolean(state, 2));
  auto flags = static_cast<ImGuiTabItemFlags>(luaL_optinteger(state, 3, 0));

  auto result = ImGui::BeginTabItem(label, &open, flags);

  lua_pushboolean(state, static_cast<int>(result));
  lua_pushboolean(state, static_cast<int>(open));
  return 2;
}
auto EndTabItem(lua_State *state) -> int {
  ImGui::EndTabItem();
  return 0;
}

auto TreeNode(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto result = ImGui::TreeNode("%s", "%s", label);
  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto TreeNodeEx(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto flags = static_cast<ImGuiTreeNodeFlags>(luaL_optinteger(state, 2, 0));
  auto result = ImGui::TreeNodeEx("%s", flags, "%s", label);
  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto TreePop(lua_State *state) -> int {
  ImGui::TreePop();
  return 0;
}

auto CollapsingHeader(lua_State *state) -> int {
  const auto *label = luaL_checkstring(state, 1);
  auto flags = static_cast<ImGuiTreeNodeFlags>(luaL_optinteger(state, 2, 0));
  auto result = ImGui::CollapsingHeader(label, flags);
  lua_pushboolean(state, static_cast<int>(result));
  return 1;
}
auto SetNextItemOpen(lua_State *state) -> int {
  auto is_open = static_cast<bool>(lua_toboolean(state, 1));
  auto cond = static_cast<ImGuiCond>(luaL_optinteger(state, 2, 0));
  ImGui::SetNextItemOpen(is_open, cond);
  return 0;
}

/*
auto GetIO(lua_State *state) -> int;
auto GetPlatformIO(lua_State *state) -> int;
auto GetStyle(lua_State *state) -> int;
auto GetFontAtlasAsRGBA32(lua_State *state) -> int;
auto GetFontAtlasAsAlpha8(lua_State *state) -> int;
auto GetDrawData(lua_State *state) -> int;
auto GetTextureID(lua_State *state) -> int;
*/

auto GetIO(lua_State *state) -> int {
  auto &inputOutput = ImGui::GetIO();
  LuaWrap::PushPointer(state, &inputOutput);
  return 1;
}

auto GetPlatformIO(lua_State *state) -> int {
  auto &platformIO = ImGui::GetPlatformIO();
  LuaWrap::PushPointer(state, &platformIO);
  return 1;
}

auto GetStyle(lua_State *state) -> int {
  auto &style = ImGui::GetStyle();
  LuaWrap::PushPointer(state, &style);
  return 1;
}

auto GetFont(lua_State *state) -> int {
  auto inputOutput = ImGui::GetIO();
  if (inputOutput.Fonts->Fonts.empty()) {
    return luaL_error(state, "No fonts loaded in ImGui IO");
  }

  auto *font = inputOutput.Fonts->Fonts[0];
  LuaWrap::PushPointer(state, font);
  return 1;
}

auto GetFontAtlas(lua_State *state) -> int {
  auto inputOutput = ImGui::GetIO();

  auto *atlas = inputOutput.Fonts;

  if (atlas == nullptr) {
    return luaL_error(state, "No font atlas available in ImGui IO");
  }

  LuaWrap::PushPointer(state, atlas);
  return 1;
}

auto NewFrame(lua_State *state) -> int {
  auto deltaTime = static_cast<float>(luaL_checknumber(state, 1));
  auto &inout = ImGui::GetIO();
  auto *ctx = Graphics::GetCurrentGraphicsContext();

  inout.DisplaySize.x = static_cast<float>(ctx->swapchainInfo.extent.width);
  inout.DisplaySize.y = static_cast<float>(ctx->swapchainInfo.extent.height);

  inout.DeltaTime = deltaTime;

  if (inout.WantSetMousePos) {
    Mouse::SetPosition(inout.MousePos.x, inout.MousePos.y);
  }

  ImGui::NewFrame();
  return 0;
}

auto EndFrame(lua_State *state) -> int {
  ImGui::EndFrame();
  return 0;
}

const std::unordered_map<ImGuiMouseCursor, Ref<Mouse::MouseCursor>>
    imgui_cursor_to_mouse_cursor{
        {ImGuiMouseCursor_Arrow, Mouse::CreateSystemCursor("arrow")},
        {ImGuiMouseCursor_TextInput, Mouse::CreateSystemCursor("text_input")},
        {ImGuiMouseCursor_ResizeAll, Mouse::CreateSystemCursor("resize_all")},
        {ImGuiMouseCursor_ResizeNS, Mouse::CreateSystemCursor("resize_ns")},
        {ImGuiMouseCursor_ResizeEW, Mouse::CreateSystemCursor("resize_ew")},
        {ImGuiMouseCursor_ResizeNESW, Mouse::CreateSystemCursor("resize_nesw")},
        {ImGuiMouseCursor_ResizeNWSE, Mouse::CreateSystemCursor("resize_nwse")},
        {ImGuiMouseCursor_Hand, Mouse::CreateSystemCursor("hand")},
        {ImGuiMouseCursor_NotAllowed, Mouse::CreateSystemCursor("not_allowed")},
    };

struct TemporaryCommandList {
  int32_t MaxVertexCount = INT32_MIN;
  int32_t MaxIndexCount = INT32_MIN;
  ImDrawList *DrawList = nullptr;

  Ref<Graphics::Mesh> Mesh;
};

const Graphics::VertexFormat format{{
    Graphics::VertexComponent{.name = "Position",
                              .location = 0,
                              .binding = 0,
                              .format = VK_FORMAT_R32G32_SFLOAT},
    Graphics::VertexComponent{.name = "UV",
                              .location = 1,
                              .binding = 0,
                              .format = VK_FORMAT_R32G32_SFLOAT},
    Graphics::VertexComponent{.name = "Color",
                              .location = 2,
                              .binding = 0,
                              .format = VK_FORMAT_R8G8B8A8_UNORM},
}};

auto Draw(lua_State *state) -> int {
  static std::vector<TemporaryCommandList> temporaryCommandLists;

  ImGui::Render();

  // Draw lists

  auto ctx = *Graphics::GetCurrentGraphicsContext();

  auto inout = ImGui::GetIO();

  if ((static_cast<uint32_t>(inout.ConfigFlags) &
       ImGuiConfigFlags_NoMouseCursorChange) == 0) {
    auto imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || inout.MouseDrawCursor) {
      Mouse::SetVisible(false);
    } else {
      Mouse::SetVisible(true);
      const auto &iterator = imgui_cursor_to_mouse_cursor.find(imgui_cursor);
      if (iterator != imgui_cursor_to_mouse_cursor.end()) {
        Mouse::SetCursor(iterator->second);
      } else {
        return luaL_error(state, "Unmapped ImGui mouse cursor type");
      }
    }
  }

  auto *drawData = ImGui::GetDrawData();

  if (drawData == nullptr) {
    return luaL_error(state, "ImGui draw data is null");
  }

  Graphics::RenderTarget::EndRendering(ctx);

  for (ImTextureData *tex : *drawData->Textures) {
    if (tex->Status == ImTextureStatus_WantCreate) {
      PrintAlways("Creating new ImGui Texture: {}x{} ({} bpp), Frame: {}",
                  tex->Width, tex->Height, tex->BytesPerPixel,
                  ctx.currentFrame);

      Graphics::Texture::TextureCreationInfo createInfo{
          .width = static_cast<uint32_t>(tex->Width),
          .height = static_cast<uint32_t>(tex->Height),
          .depth = 1,
          .format = tex->BytesPerPixel == 1 ? VK_FORMAT_R8_UNORM
                                            : VK_FORMAT_R8G8B8A8_UNORM,
          .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                   static_cast<uint32_t>(VK_IMAGE_USAGE_TRANSFER_DST_BIT),
          .mipmapCount = 1,
      };

      auto textureCreationResult = Graphics::Texture::Create2D(ctx, createInfo);

      if (Error::IsError(textureCreationResult)) {
        return luaL_error(state, "Error creating requested ImGui Texture: %s",
                          textureCreationResult.error().message.c_str());
      }

      auto *texture = textureCreationResult.value().get();

      tex->SetTexID( // NOLINTNEXTLINE reinterpret-cast
          reinterpret_cast<ImTextureID>(texture));

      texture->retain(); // Owned by ImGui now
      texture->SetFilter(VK_FILTER_NEAREST, VK_FILTER_NEAREST,
                         VK_SAMPLER_MIPMAP_MODE_NEAREST);

      auto pixelSpan = std::span<uint8_t>(
          tex->Pixels,
          static_cast<size_t>(tex->Width * tex->Height * tex->BytesPerPixel));

      auto imagedataResult = Image::ImageData::Create(
          tex->Width, tex->Height,
          tex->BytesPerPixel == 1 ? VK_FORMAT_R8_UNORM
                                  : VK_FORMAT_R8G8B8A8_UNORM);

      if (Error::IsError(imagedataResult)) {
        return luaL_error(state,
                          "Failed to create ImGui texture image data: %s",
                          imagedataResult.error().message.c_str());
      }

      auto imagedata = *imagedataResult.value();
      std::memcpy(imagedata.GetDataPtr(), pixelSpan.data(),
                  pixelSpan.size_bytes());

      auto setPixelsResult = texture->SetPixels(ctx, imagedata);

      if (Error::IsError(setPixelsResult)) {
        return luaL_error(state, "Failed to set ImGui texture pixels: %s",
                          setPixelsResult.message.c_str());
      }

      tex->SetStatus(ImTextureStatus_OK);
    } else if (tex->Status == ImTextureStatus_WantDestroy) {
      auto *texture = // NOLINTNEXTLINE reinterpret-cast
          reinterpret_cast<Graphics::Texture::Texture *>(tex->GetTexID());

      texture->release(); // Release ImGui reference

      tex->SetStatus(ImTextureStatus_Destroyed);
    }
  }

  for (int i = 0; drawData->CmdListsCount > i; ++i) {
    auto *commandList = drawData->CmdLists[i];
    if (temporaryCommandLists.size() <= i) {
      temporaryCommandLists.emplace_back();
    }

    auto &temporaryCommandList = temporaryCommandLists[i];
    temporaryCommandList.DrawList = commandList;

    auto vertexCount = commandList->VtxBuffer.Size;
    auto indexCount = commandList->IdxBuffer.Size;

    if (vertexCount == 0 || indexCount == 0) {
      continue;
    }

    if (vertexCount > temporaryCommandList.MaxVertexCount) {
      temporaryCommandList.MaxVertexCount = vertexCount;

      if (temporaryCommandList.Mesh.get() != nullptr) {
        temporaryCommandList.Mesh->release();
      }

      auto meshCreationResult =
          Graphics::Mesh::Create(ctx, format, vertexCount);

      if (Error::IsError(meshCreationResult)) {
        return luaL_error(state,
                          "Failed to create ImGui temporary vertex mesh: %s",
                          meshCreationResult.error().message.c_str());
      }

      temporaryCommandList.Mesh = meshCreationResult.value();
      PrintAlways("Created new mesh for imgui elements. Vertex Count: {}",
                  vertexCount);
    }

    auto vertexSpan =
        std::span<ImDrawVert>(commandList->VtxBuffer.Data,
                              static_cast<size_t>(commandList->VtxBuffer.Size));

    auto rawVertexSpan = std::span<uint8_t>(
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<uint8_t *>(vertexSpan.data()),
        vertexSpan.size_bytes());

    auto setResult =
        temporaryCommandList.Mesh->SetVertices(ctx, rawVertexSpan, 0);
    if (Error::IsError(setResult)) {
      return luaL_error(state,
                        "Failed to set ImGui temporary vertex mesh data: %s",
                        setResult.message.c_str());
    }

    auto indexSpan = std::span<uint8_t>( // NOLINTNEXTLINE
        reinterpret_cast<uint8_t *>(commandList->IdxBuffer.Data),
        static_cast<size_t>(commandList->IdxBuffer.Size * 2));

    setResult = temporaryCommandList.Mesh->SetIndices(
        ctx, indexSpan, Graphics::IndexFormat::Uint16);
    if (Error::IsError(setResult)) {
      return luaL_error(state,
                        "Failed to set ImGui temporary index mesh data: %s",
                        setResult.message.c_str());
    }
  }

  for (int i = 0; drawData->CmdListsCount > i; ++i) {
    auto &temporaryCommandList = temporaryCommandLists[i];
    auto *commandList = temporaryCommandList.DrawList;

    for (int cmd_i = 0; commandList->CmdBuffer.Size > cmd_i; ++cmd_i) {
      const auto &pcmd = commandList->CmdBuffer[cmd_i];

      if (pcmd.UserCallback != nullptr) {
        pcmd.UserCallback(commandList, &pcmd);
      } else {
        VkRect2D scissorRect{
            .offset{
                .x = static_cast<int32_t>(pcmd.ClipRect.x),
                .y = static_cast<int32_t>(pcmd.ClipRect.y),
            },
            .extent{
                .width =
                    static_cast<uint32_t>(pcmd.ClipRect.z - pcmd.ClipRect.x),
                .height =
                    static_cast<uint32_t>(pcmd.ClipRect.w - pcmd.ClipRect.y),
            },
        };
        Graphics::RenderTarget::SetScissor(&scissorRect);

        auto *texture = // NOLINTNEXTLINE
            reinterpret_cast<Graphics::Texture::Texture *>(pcmd.GetTexID());

        if (texture != nullptr) {
          Graphics::RenderTarget::SetShader(::Gui::ImGuiShaderRGBA8);
        } else {
          Graphics::RenderTarget::SetShader(::Gui::ImGuiShaderA8);
        }

        auto shader = Graphics::RenderTarget::GetShader();
        auto sendResult = shader->Send(ctx, {"MainTexture"}, texture);
        if (Error::IsError(sendResult)) {
          return luaL_error(state, "Failed to send ImGui texture to shader: %s",
                            sendResult.message.c_str());
        }

        temporaryCommandList.Mesh->SetDrawRange({
            .Offset = static_cast<uint32_t>(pcmd.IdxOffset),
            .Count = static_cast<uint32_t>(pcmd.ElemCount),
        });

        auto drawResult = Graphics::Draw(ctx, *temporaryCommandList.Mesh);

        if (Error::IsError(drawResult)) {
          return luaL_error(state, "Failed to draw ImGui command: %s",
                            drawResult.message.c_str());
        }
      }
    }
  }

  return 0;
}

// Imgui event passthrough functions
auto MousePressed(lua_State *state) -> int {
  // x, y, button: 1, 2, 3
  auto x_position = static_cast<float>(luaL_checknumber(state, 1));
  auto y_position = static_cast<float>(luaL_checknumber(state, 2));
  auto button = static_cast<int>(luaL_checkinteger(state, 3)) - 1; // 0-based

  auto &inout = ImGui::GetIO();
  // inout.MousePos.x = x_position;
  // inout.MousePos.y = y_position;
  inout.AddMousePosEvent(x_position, y_position);

  // NOLINTBEGIN
  if (button >= 0 && button < IM_ARRAYSIZE(inout.MouseDown)) {
    // inout.MouseDown[button] = true;
    inout.AddMouseButtonEvent(button, true);
    PrintAlways("ImGui MousePressed: button {} at {}, {}", button, x_position,
                y_position);
  }
  // NOLINTEND

  return 0;
}
auto MouseReleased(lua_State *state) -> int {
  // x, y, button: 1, 2, 3
  auto x_position = static_cast<float>(luaL_checknumber(state, 1));
  auto y_position = static_cast<float>(luaL_checknumber(state, 2));
  auto button = static_cast<int>(luaL_checkinteger(state, 3)) - 1; // 0-based

  auto &inout = ImGui::GetIO();
  inout.AddMousePosEvent(x_position, y_position);
  // NOLINTBEGIN
  if (button >= 0 && button < IM_ARRAYSIZE(inout.MouseDown)) {
    // inout.MouseDown[button] = false;
    inout.AddMouseButtonEvent(button, false);
  }
  // NOLINTEND

  return 0;
}
auto MouseMoved(lua_State *state) -> int {
  // x, y
  auto x_position = static_cast<float>(luaL_checknumber(state, 1));
  auto y_position = static_cast<float>(luaL_checknumber(state, 2));

  auto &inout = ImGui::GetIO();
  inout.AddMousePosEvent(x_position, y_position);

  return 0;
}

auto KeyPressed(lua_State *state) -> int {
  // key is a, b, c, lshift, rshift, ctrl, alt, etc.
  const auto *key = luaL_checkstring(state, 1);

  // use keyboard module to get SDL keycode
  auto keycodeMap = Keyboard::StringToKeycode;
  auto iterator = keycodeMap.find(key);
  if (iterator == keycodeMap.end()) {
    return luaL_error(state,
                      "Unknown key string passed to ImGui KeyPressed: %s", key);
  }

  auto keycode = static_cast<SDL_Keycode>(iterator->second);

  auto scanecodeMap = Keyboard::StringToScancode;
  auto scancodeIterator = scanecodeMap.find(key);
  if (scancodeIterator == scanecodeMap.end()) {
    return luaL_error(state,
                      "Unknown key string passed to ImGui KeyPressed: %s", key);
  }

  auto scancode = static_cast<SDL_Scancode>(scancodeIterator->second);

  auto &inout = ImGui::GetIO();
  auto imkey = ::Gui::KeyEventToImguiKey(keycode, scancode);

  inout.AddKeyEvent(imkey, true);

  return 0;
}
auto KeyReleased(lua_State *state) -> int {
  // key is a, b, c, lshift, rshift, ctrl, alt, etc.
  const auto *key = luaL_checkstring(state, 1);

  // use keyboard module to get SDL keycode
  auto keycodeMap = Keyboard::StringToKeycode;
  auto iterator = keycodeMap.find(key);
  if (iterator == keycodeMap.end()) {
    return luaL_error(
        state, "Unknown key string passed to ImGui KeyReleased: %s", key);
  }

  auto keycode = static_cast<SDL_Keycode>(iterator->second);

  auto scanecodeMap = Keyboard::StringToScancode;
  auto scancodeIterator = scanecodeMap.find(key);
  if (scancodeIterator == scanecodeMap.end()) {
    return luaL_error(
        state, "Unknown key string passed to ImGui KeyReleased: %s", key);
  }

  auto scancode = static_cast<SDL_Scancode>(scancodeIterator->second);

  auto &inout = ImGui::GetIO();
  auto imkey = ::Gui::KeyEventToImguiKey(keycode, scancode);

  inout.AddKeyEvent(imkey, false);

  return 0;
}
auto TextInput(lua_State *state) -> int {
  // text
  const auto *text = luaL_checkstring(state, 1);

  auto &inout = ImGui::GetIO();
  inout.AddInputCharactersUTF8(text);

  return 0;
}
auto MouseWheelMoved(lua_State *state) -> int {
  // x, y
  auto x_scroll = static_cast<float>(luaL_checknumber(state, 1));
  auto y_scroll = static_cast<float>(luaL_checknumber(state, 2));

  auto &inout = ImGui::GetIO();
  inout.AddMouseWheelEvent(x_scroll, y_scroll);

  return 0;
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
} // namespace Wrap::Imgui