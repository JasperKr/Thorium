#include "gui.hpp"
#include "Modules/console.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include "SDL3/SDL_clipboard.h"
#include "SDL3/SDL_mouse.h"
#include "imgui.h"
#include "imstb_truetype.h"
#include <array>
#include <cassert>
#include <unordered_map>

namespace Gui {

// NOLINTBEGIN
Ref<Graphics::Shader::ShaderModule> ImGuiShaderRGBA8;
Ref<Graphics::Shader::ShaderModule> ImGuiShaderA8;
// NOLINTEND

auto MainWindow() -> void {}

auto LoadGUIState(lua_State *state) -> Result<GuiState> {
  auto loadResult = LoadImGuiCursorMap();
  if (Error::IsError(loadResult)) {
    return loadResult.AsUnexpected();
  }

  GuiState guiState{};

  // Load GUI state from Lua
  if (luaL_dostring(state, luaStateDefinition.c_str()) != LUA_OK) {
    const char *errorMessage = lua_tostring(state, -1);
    lua_pop(state, 1); // Remove error message from stack

    return Error::Unexpected(
        "Failed to load GUI state definition: " +
        std::string(errorMessage != nullptr ? errorMessage : "Unknown error"));
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGui::StyleColorsDark();

  auto &inout = ImGui::GetIO();
  auto flags = static_cast<uint32_t>(inout.ConfigFlags);
  flags |= static_cast<uint32_t>(
      ImGuiConfigFlags_NavEnableKeyboard); // Enable Keyboard Controls
  inout.ConfigFlags = static_cast<ImGuiConfigFlags>(flags);

  auto &platformIO = ImGui::GetPlatformIO();
  platformIO.Platform_GetClipboardTextFn =
      [](ImGuiContext *context) -> const char * {
    return SDL_GetClipboardText();
  };
  platformIO.Platform_SetClipboardTextFn = [](ImGuiContext *context,
                                              const char *text) -> void {
    SDL_SetClipboardText(text);
  };

  float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  assert(scale > 0.0F && "Display content scale must be greater than 0");

  ImGuiStyle &style = ImGui::GetStyle();
  style.FontScaleDpi = scale;
  style.ScaleAllSizes(scale);

  const float baseFontSize = 18.0F;

  ImFontConfig config;
  config.OversampleH = 5;
  config.OversampleV = 5;
  config.PixelSnapH = true;
  config.SizePixels = baseFontSize * scale;

  ImGui::GetIO().Fonts->AddFontDefault(&config);

  ImFont *font = inout.Fonts->AddFontFromFileTTF(
      "src/Graphics/Assets/user_interface_font.ttf", baseFontSize, &config);

  // Set the font as the default font
  inout.FontDefault = font;

  ImGuiIO &io = ImGui::GetIO();

  flags = static_cast<uint32_t>(inout.BackendFlags);
  flags |= static_cast<uint32_t>(ImGuiBackendFlags_RendererHasTextures);
  flags |= static_cast<uint32_t>(ImGuiBackendFlags_HasSetMousePos);
  flags |= static_cast<uint32_t>(ImGuiBackendFlags_HasMouseCursors);
  inout.BackendFlags = static_cast<ImGuiBackendFlags>(flags);

  auto ctx = *Graphics::GetCurrentGraphicsContext();

  auto rgba8CreationResult = Graphics::Shader::ShaderModule::Create(
      ctx, "Scripting/Graphics/Shaders/ImGuiRGBA8", "Imgui rgba8 shader");
  if (Error::IsError(rgba8CreationResult)) {
    return Error::Unexpected("Failed to create ImGui RGBA8 shader: " +
                             rgba8CreationResult.error().message);
  }

  auto a8CreationResult = Graphics::Shader::ShaderModule::Create(
      ctx, "Scripting/Graphics/Shaders/ImGuiA8", "Imgui a8 shader");
  if (Error::IsError(a8CreationResult)) {
    return Error::Unexpected("Failed to create ImGui A8 shader: " +
                             a8CreationResult.error().message);
  }

  ImGuiShaderRGBA8 = rgba8CreationResult.value();
  ImGuiShaderA8 = a8CreationResult.value();

  return guiState;
}

auto LoadImGuiCursorMap() -> Error {
  auto &map = GetImGuiCursorMap();

  struct CursorMapping {
    int imguiCursor;
    SDL_SystemCursor sdlCursor;
  };

  static const std::array<CursorMapping, 11> mappings{
      CursorMapping{.imguiCursor = ImGuiMouseCursor_Arrow,
                    .sdlCursor = SDL_SYSTEM_CURSOR_DEFAULT},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_TextInput,
                    .sdlCursor = SDL_SYSTEM_CURSOR_TEXT},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_ResizeAll,
                    .sdlCursor = SDL_SYSTEM_CURSOR_MOVE},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_ResizeNS,
                    .sdlCursor = SDL_SYSTEM_CURSOR_NS_RESIZE},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_ResizeEW,
                    .sdlCursor = SDL_SYSTEM_CURSOR_EW_RESIZE},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_ResizeNESW,
                    .sdlCursor = SDL_SYSTEM_CURSOR_NESW_RESIZE},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_ResizeNWSE,
                    .sdlCursor = SDL_SYSTEM_CURSOR_NWSE_RESIZE},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_Hand,
                    .sdlCursor = SDL_SYSTEM_CURSOR_POINTER},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_Wait,
                    .sdlCursor = SDL_SYSTEM_CURSOR_WAIT},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_Progress,
                    .sdlCursor = SDL_SYSTEM_CURSOR_PROGRESS},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_NotAllowed,
                    .sdlCursor = SDL_SYSTEM_CURSOR_NOT_ALLOWED},
  };

  for (const auto &mapping : mappings) {
    auto result = Mouse::CreateSystemCursor(mapping.sdlCursor);
    if (Error::IsError(result)) {
      return result.error();
    }
    map[mapping.imguiCursor] = result.value();
  }

  return Error::Success();
}

std::unordered_map<ImTextureID, Ref<Graphics::Texture::Texture>>
    ImGuiTextures{}; // NOLINT

auto ShutdownImGui() -> Error {
  ImGui::DestroyContext();
  ImGuiTextures.clear();

  ImGuiShaderRGBA8.reset(); // Release reference
  ImGuiShaderA8.reset();

  return Error::Success();
}

} // namespace Gui