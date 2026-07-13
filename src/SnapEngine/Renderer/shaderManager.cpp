#include "shaderManager.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/shader.hpp"

namespace Engine::Renderer {

auto ShaderManager::GetShader(ShaderKey shaderKey)
    -> Result<Ref<Graphics::Shader>> {
  auto iterator = LoadedShaders.find(shaderKey);
  if (iterator != LoadedShaders.end()) {
    return iterator->second;
  }

  auto configurationIter = ShaderConfigurations.find(shaderKey);
  if (configurationIter == ShaderConfigurations.end()) {
    auto intKey = static_cast<int>(shaderKey);
    return Error::Unexpectedf("Shader not found for key: {}", intKey);
  }

  const auto &configuration = configurationIter->second;
  auto context = *Graphics::GetCurrentGraphicsContext();

  auto shaderModule = CHECK_RES(Graphics::Shader::Create(
      context, configuration.path, configuration.name, &configuration.Externs));

  LoadedShaders[shaderKey] = shaderModule;

  return shaderModule;
}

auto ShaderManager::ReloadShaders() -> void { LoadedShaders.clear(); }

} // namespace Engine::Renderer