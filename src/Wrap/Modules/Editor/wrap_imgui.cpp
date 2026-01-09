#include "wrap_imgui.hpp"
#include "Graphics/texture.hpp"
#include "Modules/imagedata.hpp"
#include "Wrap/wrap.hpp"
#include "imgui.h"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

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

auto GetDrawData(lua_State *state) -> int {
  auto *drawData = ImGui::GetDrawData();
  LuaWrap::PushPointer(state, drawData);
  return 1;
}

auto NewFrame(lua_State *state) -> int {
  ImGui::NewFrame();
  return 0;
}

auto GetDrawLists(lua_State *state) -> int {
  auto *drawData = ImGui::GetDrawData();
  LuaWrap::PushPointer(state, drawData);
  return 1;
}

auto EndFrame(lua_State *state) -> int {
  ImGui::EndFrame();
  return 0;
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
} // namespace Wrap::Imgui