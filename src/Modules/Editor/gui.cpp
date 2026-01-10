#include "gui.hpp"
#include "Modules/filesystem.hpp"

namespace Gui {

// NOLINTBEGIN
Ref<Graphics::Shader::ShaderModule> ImGuiShaderRGBA8;
Ref<Graphics::Shader::ShaderModule> ImGuiShaderA8;
// NOLINTEND

auto MainWindow() -> void {}

auto LoadGUIState(lua_State *state) -> Result<GuiState> {
  GuiState guiState{};

  // Load GUI state from Lua
  if (luaL_dostring(state, luaStateDefinition.c_str()) != LUA_OK) {
    const char *errorMessage = lua_tostring(state, -1);
    lua_pop(state, 1); // Remove error message from stack

    return Error::Unexpected(
        "Failed to load GUI state definition: " +
        std::string(errorMessage != nullptr ? errorMessage : "Unknown error"));
  }

  auto *guiContext = ImGui::CreateContext();

  GetGuiState() = guiState;

  ImGui::StyleColorsDark();

  auto &inout = ImGui::GetIO();
  auto flags = static_cast<uint32_t>(inout.ConfigFlags);
  flags |= static_cast<uint32_t>(
      ImGuiConfigFlags_NavEnableKeyboard); // Enable Keyboard Controls
  inout.ConfigFlags = static_cast<ImGuiConfigFlags>(flags);

  flags = static_cast<uint32_t>(inout.BackendFlags);
  flags |= static_cast<uint32_t>(ImGuiBackendFlags_RendererHasTextures);
  inout.BackendFlags = static_cast<ImGuiBackendFlags>(flags);

  auto fontDataResult =
      Filesystem::ReadFile("src/Graphics/Assets/user_interface_font.ttf");
  if (Error::IsError(fontDataResult)) {
    return Error::Unexpected("Failed to read GUI font file: " +
                             fontDataResult.error().message);
  }

  auto fontData = fontDataResult.value();

  PrintAlways("Loaded GUI font file, size: {} bytes", fontData.size());

  const float fontSize = 16.0F;
  const std::string debugname = "Source Code Pro - Mono";

  auto baseConfig = ImFontConfig();
  baseConfig.FontDataOwnedByAtlas = false;
  // baseConfig.Name = "Source Code Pro - Mono";
  // NOLINTNEXTLINE
  std::strncpy(baseConfig.Name, debugname.c_str(), sizeof(baseConfig.Name) - 1);
  baseConfig.MergeMode = false;
  baseConfig.PixelSnapH = true;
  baseConfig.OversampleH = 5; // NOLINT
  baseConfig.OversampleV = 5; // NOLINT

  ImFont *font = inout.Fonts->AddFontFromMemoryTTF(
      fontData.data(), static_cast<int>(fontData.size()), fontSize,
      &baseConfig);
  if (font == nullptr) {
    return Error::Unexpected("Failed to load GUI font from memory.");
  }

  // Optionally set this font as default
  inout.FontDefault = font;

  auto ctx = *Graphics::GetCurrentGraphicsContext();

  auto rgba8CreationResult = Graphics::Shader::ShaderModule::Create(
      ctx, "ImGuiRGBA8", "Imgui rgba8 shader");
  if (Error::IsError(rgba8CreationResult)) {
    return Error::Unexpected("Failed to create ImGui RGBA8 shader: " +
                             rgba8CreationResult.error().message);
  }

  auto a8CreationResult =
      Graphics::Shader::ShaderModule::Create(ctx, "ImGuiA8", "Imgui a8 shader");
  if (Error::IsError(a8CreationResult)) {
    return Error::Unexpected("Failed to create ImGui A8 shader: " +
                             a8CreationResult.error().message);
  }

  ImGuiShaderRGBA8 = rgba8CreationResult.value();
  ImGuiShaderA8 = a8CreationResult.value();

  return guiState;
}

} // namespace Gui