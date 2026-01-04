#pragma once

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Wrap::Imgui {
auto Begin(lua_State *state) -> int;
auto End(lua_State *state) -> int;

auto BeginChild(lua_State *state) -> int;
auto EndChild(lua_State *state) -> int;

auto BeginGroup(lua_State *state) -> int;
auto EndGroup(lua_State *state) -> int;

auto Separator(lua_State *state) -> int;
auto SeparatorText(lua_State *state) -> int;

auto Dummy(lua_State *state) -> int;
auto Spacing(lua_State *state) -> int;
auto NewLine(lua_State *state) -> int;
auto Indent(lua_State *state) -> int;
auto Unindent(lua_State *state) -> int;
auto SameLine(lua_State *state) -> int;

auto GetCursorPos(lua_State *state) -> int;
auto SetCursorPos(lua_State *state) -> int;
auto SetCursorPosX(lua_State *state) -> int;
auto SetCursorPosY(lua_State *state) -> int;

auto GetCursorScreenPos(lua_State *state) -> int;
auto SetCursorScreenPos(lua_State *state) -> int;
auto GetCursorStartPos(lua_State *state) -> int;

auto GetContentRegionAvail(lua_State *state) -> int;
auto GetContentRegionMax(lua_State *state) -> int;

auto GetWindowContentRegionMin(lua_State *state) -> int;
auto GetWindowContentRegionMax(lua_State *state) -> int;

auto GetWindowPos(lua_State *state) -> int;
auto GetWindowSize(lua_State *state) -> int;
auto SetWindowPos(lua_State *state) -> int;
auto SetWindowSize(lua_State *state) -> int;
auto SetWindowCollapsed(lua_State *state) -> int;
auto SetWindowFocus(lua_State *state) -> int;

auto IsWindowFocused(lua_State *state) -> int;
auto IsWindowCollapsed(lua_State *state) -> int;

auto GetWindowWidth(lua_State *state) -> int;
auto GetWindowHeight(lua_State *state) -> int;
auto GetWindowDrawList(lua_State *state) -> int;

auto GetScrollX(lua_State *state) -> int;
auto GetScrollY(lua_State *state) -> int;
auto SetScrollX(lua_State *state) -> int;
auto SetScrollY(lua_State *state) -> int;

auto Text(lua_State *state) -> int;
auto TextColored(lua_State *state) -> int;
auto TextDisabled(lua_State *state) -> int;
auto TextWrapped(lua_State *state) -> int;
auto LabelText(lua_State *state) -> int;
auto BulletText(lua_State *state) -> int;
auto Bullet(lua_State *state) -> int;

auto Button(lua_State *state) -> int;
auto SmallButton(lua_State *state) -> int;
auto InvisibleButton(lua_State *state) -> int;
auto ArrowButton(lua_State *state) -> int;
auto ImageButton(lua_State *state) -> int;
auto Image(lua_State *state) -> int;
auto Checkbox(lua_State *state) -> int;
auto RadioButton(lua_State *state) -> int;
auto ProgressBar(lua_State *state) -> int;

auto SliderFloat(lua_State *state) -> int;
auto SliderInt(lua_State *state) -> int;
auto VSliderFloat(lua_State *state) -> int;
auto VSliderInt(lua_State *state) -> int;

auto DragFloat(lua_State *state) -> int;
auto DragInt(lua_State *state) -> int;
auto DragFloat2(lua_State *state) -> int;
auto DragInt2(lua_State *state) -> int;
auto DragFloat3(lua_State *state) -> int;
auto DragInt3(lua_State *state) -> int;
auto DragFloat4(lua_State *state) -> int;
auto DragInt4(lua_State *state) -> int;

auto InputText(lua_State *state) -> int;
auto InputTextMultiline(lua_State *state) -> int;
auto InputFloat(lua_State *state) -> int;
auto InputInt(lua_State *state) -> int;
auto InputFloat2(lua_State *state) -> int;
auto InputInt2(lua_State *state) -> int;
auto InputFloat3(lua_State *state) -> int;
auto InputInt3(lua_State *state) -> int;
auto InputFloat4(lua_State *state) -> int;
auto InputInt4(lua_State *state) -> int;

auto ColorEdit3(lua_State *state) -> int;
auto ColorEdit4(lua_State *state) -> int;
auto ColorPicker3(lua_State *state) -> int;
auto ColorPicker4(lua_State *state) -> int;

auto BeginCombo(lua_State *state) -> int;
auto EndCombo(lua_State *state) -> int;

auto BeginTabBar(lua_State *state) -> int;
auto EndTabBar(lua_State *state) -> int;
auto BeginTabItem(lua_State *state) -> int;
auto EndTabItem(lua_State *state) -> int;

auto TreeNode(lua_State *state) -> int;
auto TreeNodeEx(lua_State *state) -> int;
auto TreePop(lua_State *state) -> int;

auto CollapsingHeader(lua_State *state) -> int;
auto SetNextItemOpen(lua_State *state) -> int;

// NOLINTNEXTLINE
static const luaL_Reg ImGuiLib[] = {
    {"begin", Begin},
    {"end", End},
    {"beginChild", BeginChild},
    {"endChild", EndChild},
    {"beginGroup", BeginGroup},
    {"endGroup", EndGroup},
    {"separator", Separator},
    {"separatorText", SeparatorText},
    {"dummy", Dummy},
    {"spacing", Spacing},
    {"newLine", NewLine},
    {"indent", Indent},
    {"unindent", Unindent},
    {"sameLine", SameLine},
    {"getCursorPos", GetCursorPos},
    {"setCursorPos", SetCursorPos},
    {"setCursorPosX", SetCursorPosX},
    {"setCursorPosY", SetCursorPosY},
    {"getCursorScreenPos", GetCursorScreenPos},
    {"setCursorScreenPos", SetCursorScreenPos},
    {"getCursorStartPos", GetCursorStartPos},
    {"getContentRegionAvail", GetContentRegionAvail},
    {"getContentRegionMax", GetContentRegionMax},
    {"getWindowContentRegionMin", GetWindowContentRegionMin},
    {"getWindowContentRegionMax", GetWindowContentRegionMax},
    {"getWindowPos", GetWindowPos},
    {"getWindowSize", GetWindowSize},
    {"setWindowPos", SetWindowPos},
    {"setWindowSize", SetWindowSize},
    {"setWindowCollapsed", SetWindowCollapsed},
    {"setWindowFocus", SetWindowFocus},
    {"isWindowFocused", IsWindowFocused},
    {"isWindowCollapsed", IsWindowCollapsed},
    {"getWindowWidth", GetWindowWidth},
    {"getWindowHeight", GetWindowHeight},
    {"getWindowDrawList", GetWindowDrawList},
    {"getScrollX", GetScrollX},
    {"getScrollY", GetScrollY},
    {"setScrollX", SetScrollX},
    {"setScrollY", SetScrollY},
    {"text", Text},
    {"textColored", TextColored},
    {"textDisabled", TextDisabled},
    {"textWrapped", TextWrapped},
    {"labelText", LabelText},
    {"bulletText", BulletText},
    {"bullet", Bullet},
    {"button", Button},
    {"smallButton", SmallButton},
    {"invisibleButton", InvisibleButton},
    {"arrowButton", ArrowButton},
    {"imageButton", ImageButton},
    {"image", Image},
    {"checkbox", Checkbox},
    {"radioButton", RadioButton},
    {"progressBar", ProgressBar},
    {"sliderFloat", SliderFloat},
    {"sliderInt", SliderInt},
    {"vSliderFloat", VSliderFloat},
    {"vSliderInt", VSliderInt},
    {"dragFloat", DragFloat},
    {"dragInt", DragInt},
    {"dragFloat2", DragFloat2},
    {"dragInt2", DragInt2},
    {"dragFloat3", DragFloat3},
    {"dragInt3", DragInt3},
    {"dragFloat4", DragFloat4},
    {"dragInt4", DragInt4},
    {"inputText", InputText},
    {"inputTextMultiline", InputTextMultiline},
    {"inputFloat", InputFloat},
    {"inputInt", InputInt},
    {"inputFloat2", InputFloat2},
    {"inputInt2", InputInt2},
    {"inputFloat3", InputFloat3},
    {"inputInt3", InputInt3},
    {"inputFloat4", InputFloat4},
    {"inputInt4", InputInt4},
    {"colorEdit3", ColorEdit3},
    {"colorEdit4", ColorEdit4},
    {"colorPicker3", ColorPicker3},
    {"colorPicker4", ColorPicker4},
    {"beginCombo", BeginCombo},
    {"endCombo", EndCombo},
    {"beginTabBar", BeginTabBar},
    {"endTabBar", EndTabBar},
    {"beginTabItem", BeginTabItem},
    {"endTabItem", EndTabItem},
    {"treeNode", TreeNode},
    {"treeNodeEx", TreeNodeEx},
    {"treePop", TreePop},
    {"collapsingHeader", CollapsingHeader},
    {"setNextItemOpen", SetNextItemOpen},
    {nullptr, nullptr},
};
} // namespace Wrap::Imgui