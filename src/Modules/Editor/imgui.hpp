#include "Graphics/rendergraph.hpp"
#include "imgui_internal.h"
namespace Editor {
struct Context {
  ImGuiContext *imguiContext;
};

auto InitializeImGui(
    Graphics::GraphicsContext &context,
    Graphics::Rendergraph::RenderGraph &graph,
    Graphics::Rendergraph::ResourceHandle lastResourceHandle, // NOLINT
    Graphics::Rendergraph::ResourceHandle writeResourceHandle,
    Context &editorContext) -> Error::Error;
} // namespace Editor