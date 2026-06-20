#include "textureView.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/resource.hpp"
#include "Modules/error.hpp"
#include "texture.hpp"

namespace Graphics {

auto TextureView::Create(const GraphicsContext &context,
                         const TextureViewCreateInfo &info)
    -> Result<Ref<TextureView>> {

  auto viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;

  switch (info.texture->textureType) {
  case TextureType::DEFAULT:
    viewType = VK_IMAGE_VIEW_TYPE_2D;
    break;
  case TextureType::VOLUME:
    viewType = VK_IMAGE_VIEW_TYPE_3D;
    break;
  case TextureType::CUBEMAP:
    viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    break;
  case TextureType::ARRAY:
    viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    break;
  default:
    return Error::Unexpected("Unsupported texture type for texture view");
  }

  auto imageViewCreateInfo = VkImageViewCreateInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = info.texture->image,
      .viewType = viewType,
      .format = info.texture->format,
      .subresourceRange = info.subresourceRange,
  };

  auto view = Ref<TextureView>();

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkCreateImageView(context.device, &imageViewCreateInfo,
                                    nullptr, &view->imageView));
  }
  view->texture = info.texture;
  view->subresourceRange = info.subresourceRange;

  return view;
}

auto TextureView::MarkUse() -> void {
  lastUsedTimestamp = Graphics::SemaphoreManager::GetSemaphoreValue();
}

TextureView::~TextureView() {
  if (imageView != VK_NULL_HANDLE) {
    ScheduleDestruction(TextureViewMemory{.imageView = imageView,
                                          .timelineValue = lastUsedTimestamp});
  }
}

} // namespace Graphics