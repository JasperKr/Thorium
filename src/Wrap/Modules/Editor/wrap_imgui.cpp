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
#include "imgui.h"
#include "vulkan/vulkan_core.h"
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

namespace Wrap::Imgui {

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

inline auto ChangeMouseState(ImGuiIO &inout) -> Error {
  if ((static_cast<uint32_t>(inout.ConfigFlags) &
       ImGuiConfigFlags_NoMouseCursorChange) == 0) {
    auto imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || inout.MouseDrawCursor) {
      Mouse::SetVisible(false);
    } else {
      Mouse::SetVisible(true);
      const auto &imgui_cursor_to_mouse_cursor = ::Gui::GetImGuiCursorMap();

      const auto &iterator = imgui_cursor_to_mouse_cursor.find(imgui_cursor);
      if (iterator != imgui_cursor_to_mouse_cursor.end()) {
        if (iterator->second->sdlCursor == nullptr) {
          return Error::Create("ImGui mouse cursor not created");
        }

        auto setResult = Mouse::SetCursor(iterator->second);
        if (Error::IsError(setResult)) {
          return setResult;
        }
      } else {
        return Error::Create("Unmapped ImGui mouse cursor type");
      }
    }
  }

  return Error::Success();
}

inline auto HandleImguiCreateTextureEvent(Graphics::GraphicsContext &context,
                                          ImTextureData *tex) -> Error {
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

  auto textureCreationResult = Graphics::Texture::Create2D(context, createInfo);

  if (Error::IsError(textureCreationResult)) {
    return textureCreationResult.error();
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
      tex->BytesPerPixel == 1 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8B8A8_UNORM);

  if (Error::IsError(imagedataResult)) {
    return imagedataResult.error();
  }

  auto imagedata = *imagedataResult.value();
  std::memcpy(imagedata.GetDataPtr(), pixelSpan.data(), pixelSpan.size_bytes());

  auto setPixelsResult = texture->SetPixels(context, imagedata);

  if (Error::IsError(setPixelsResult)) {
    return setPixelsResult;
  }

  tex->SetStatus(ImTextureStatus_OK);

  return Error::Success();
}

inline auto HandleImguiDestroyTextureEvent(ImTextureData *tex) -> Error {
  auto *texture = // NOLINTNEXTLINE reinterpret-cast
      reinterpret_cast<Graphics::Texture::Texture *>(tex->GetTexID());

  texture->release(); // Release ImGui reference
  tex->SetTexID(0);

  tex->SetStatus(ImTextureStatus_Destroyed);

  return Error::Success();
}

inline auto HandleImguiUpdateTextureEvent(Graphics::GraphicsContext &context,
                                          ImTextureData *tex) -> Error {
  auto *pixels = static_cast<uint8_t *>(tex->GetPixels());
  auto pixelSpan =
      std::span<uint8_t>(pixels, static_cast<size_t>(tex->Width * tex->Height *
                                                     tex->BytesPerPixel));

  auto *texture = // NOLINTNEXTLINE reinterpret-cast
      reinterpret_cast<Graphics::Texture::Texture *>(tex->GetTexID());

  for (ImTextureRect &currentRect : tex->Updates) {

    VkRect2D sourceRect{
        .offset{
            .x = static_cast<int32_t>(currentRect.x),
            .y = static_cast<int32_t>(currentRect.y),
        },
        .extent{
            .width = static_cast<uint32_t>(currentRect.w),
            .height = static_cast<uint32_t>(currentRect.h),
        },
    };

    VkOffset2D destOffset{
        .x = static_cast<int32_t>(currentRect.x),
        .y = static_cast<int32_t>(currentRect.y),
    };

    // Use the update data from the current pixels
    auto updateResult =
        texture->SetPixels(context, pixelSpan, tex->Width, tex->Height, 0, 0,
                           sourceRect, destOffset);

    if (Error::IsError(updateResult)) {
      return updateResult;
    }
  }

  tex->SetStatus(ImTextureStatus_OK);

  return Error::Success();
}

inline auto SetupTemporaryCommandLists(
    ImDrawData *drawData, Graphics::GraphicsContext &ctx,
    std::vector<TemporaryCommandList> &temporaryCommandLists) -> Error {
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
        return meshCreationResult.error();
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
      return setResult;
    }

    auto indexSpan = std::span<uint8_t>( // NOLINTNEXTLINE
        reinterpret_cast<uint8_t *>(commandList->IdxBuffer.Data),
        static_cast<size_t>(commandList->IdxBuffer.size() * 2));

    setResult = temporaryCommandList.Mesh->SetIndices(
        ctx, indexSpan, Graphics::IndexFormat::Uint16);
    if (Error::IsError(setResult)) {
      return setResult;
    }
  }

  return Error::Success();
}

inline auto DrawTemporaryCommandLists(
    Graphics::GraphicsContext &ctx, ImDrawData *drawData,
    const std::vector<TemporaryCommandList> &temporaryCommandLists) -> Error {
  for (int i = 0; drawData->CmdListsCount > i; ++i) {
    const auto &temporaryCommandList = temporaryCommandLists[i];
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
          return sendResult;
        }

        temporaryCommandList.Mesh->SetDrawRange({
            .Offset = static_cast<uint32_t>(pcmd.IdxOffset),
            .Count = static_cast<uint32_t>(pcmd.ElemCount),
        });

        auto drawResult = Graphics::Draw(ctx, *temporaryCommandList.Mesh);

        if (Error::IsError(drawResult)) {
          return drawResult;
        }
      }
    }
  }

  return Error::Success();
}

auto Draw(lua_State *state) -> int {
  static std::vector<TemporaryCommandList> temporaryCommandLists;

  ImGui::Render();

  auto ctx = *Graphics::GetCurrentGraphicsContext();

  auto inout = ImGui::GetIO();
  Graphics::RenderTarget::SetCullMode(VK_CULL_MODE_NONE);

  auto changeResult = ChangeMouseState(inout);
  if (Error::IsError(changeResult)) {
    return luaL_error(state, "Failed to change ImGui mouse state: %s",
                      changeResult.message.c_str());
  }

  auto *drawData = ImGui::GetDrawData();

  if (drawData == nullptr) {
    return luaL_error(state, "ImGui draw data is null");
  }

  Graphics::RenderTarget::EndRendering(ctx);

  for (ImTextureData *tex : *drawData->Textures) {
    if (tex->Status == ImTextureStatus_WantCreate) {
      auto creationResult = HandleImguiCreateTextureEvent(ctx, tex);
      if (Error::IsError(creationResult)) {
        return luaL_error(state, "Failed to create ImGui texture: %s",
                          creationResult.message.c_str());
      }
    } else if (tex->Status == ImTextureStatus_WantDestroy) {
      auto destructionResult = HandleImguiDestroyTextureEvent(tex);
      if (Error::IsError(destructionResult)) {
        return luaL_error(state, "Failed to destroy ImGui texture: %s",
                          destructionResult.message.c_str());
      }
    } else if (tex->Status == ImTextureStatus_WantUpdates) {
      auto updateResult = HandleImguiUpdateTextureEvent(ctx, tex);
      if (Error::IsError(updateResult)) {
        return luaL_error(state, "Failed to update ImGui texture: %s",
                          updateResult.message.c_str());
      }
    }
  }

  auto setupResult =
      SetupTemporaryCommandLists(drawData, ctx, temporaryCommandLists);

  if (Error::IsError(setupResult)) {
    return luaL_error(state,
                      "Failed to setup ImGui temporary command lists: %s",
                      setupResult.message.c_str());
  }

  auto drawResult =
      DrawTemporaryCommandLists(ctx, drawData, temporaryCommandLists);

  if (Error::IsError(drawResult)) {
    return luaL_error(state, "Failed to draw ImGui temporary command lists: %s",
                      drawResult.message.c_str());
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
  inout.AddMousePosEvent(x_position, y_position);

  // NOLINTBEGIN
  if (button >= 0 && button < IM_ARRAYSIZE(inout.MouseDown)) {
    inout.AddMouseButtonEvent(button, true);
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

} // namespace Wrap::Imgui