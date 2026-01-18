#include "Graphics/renderThread.hpp"
#include "Graphics/graphics.hpp"

// NOLINTNEXTLINE

namespace Graphics::Threading {
auto AquireCommandBuffer(Graphics::GraphicsContext &context,
                         RenderThreadInfo &threadInfo)
    -> Ref<CommandBufferResult> {
  auto result = Ref<CommandBufferResult>::Make(context);
}
auto SubmitCommands(Graphics::GraphicsContext &context,
                    VkCommandBuffer commandBuffer, RenderThreadInfo &threadInfo)
    -> Error {}
} // namespace Graphics::Threading