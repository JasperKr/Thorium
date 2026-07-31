#include "shaderManager.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/shader.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace Engine::Renderer {

// NOLINTBEGIN

std::mutex StatusMutex;
std::condition_variable CreatedCV;

// NOLINTEND

auto ShaderManager::GetShader(ShaderKey shaderKey)
    -> Result<Ref<Graphics::Shader>> {
  auto configurationIter = ShaderConfigurations.find(shaderKey);
  if (configurationIter == ShaderConfigurations.end()) {
    auto intKey = static_cast<int>(shaderKey);
    return Error::Unexpectedf("Shader not found for key: {}", intKey);
  }

  const auto &configuration = configurationIter->second;
  auto context = *Graphics::GetCurrentGraphicsContext();

  while (true) {
    std::unique_lock<std::mutex> lock(StatusMutex);
    auto [statusIter, inserted] = CompilationStatuses.try_emplace(
        shaderKey, ShaderCompilationStatus::Unloaded);

    (void)inserted;

    if (statusIter->second == ShaderCompilationStatus::Loading) {
      CreatedCV.wait(lock, [&]() -> bool {
        return CompilationStatuses.at(shaderKey) !=
               ShaderCompilationStatus::Loading;
      });

      continue;
    }

    auto shaderIter = LoadedShaders.find(shaderKey);
    if (shaderIter != LoadedShaders.end()) {
      return shaderIter->second;
    }

    statusIter->second = ShaderCompilationStatus::Loading;
    lock.unlock();

    auto shaderResult =
        Graphics::Shader::Create(context, configuration.path,
                                 configuration.name, &configuration.externs);

    lock.lock();

    if (Error::IsError(shaderResult)) {
      CompilationStatuses[shaderKey] = ShaderCompilationStatus::Unloaded;
      CreatedCV.notify_all();
      return shaderResult.error();
    }

    LoadedShaders[shaderKey] = shaderResult.value();
    CompilationStatuses[shaderKey] = ShaderCompilationStatus::Created;
    CreatedCV.notify_all();

    return shaderResult.value();
  }
}

auto ShaderManager::Preload() -> Error {
  std::vector<std::pair<ShaderKey, ShaderConfiguration const *>> configurations;
  configurations.reserve(ShaderConfigurations.size());

  for (const auto &config : ShaderConfigurations) {
    configurations.emplace_back(config.first, &config.second);
  }

  auto context = *Graphics::GetCurrentGraphicsContext();

  std::jthread compiler([this, context, configurations] mutable -> void {
    auto error = Utils::ParallelFor(
        configurations.size(), [&](const auto &index) -> Error {
          const std::pair<ShaderKey, ShaderConfiguration const *> &pair =
              configurations.at(index);

          {
            std::lock_guard<std::mutex> lock(StatusMutex);
            auto [statusIter, inserted] = CompilationStatuses.try_emplace(
                pair.first, ShaderCompilationStatus::Unloaded);

            (void)inserted;

            if (statusIter->second != ShaderCompilationStatus::Unloaded) {
              return {}; // If we arent unloaded, so created / creating, skip.
            }

            statusIter->second = ShaderCompilationStatus::Loading;
          }

          auto shaderModule = Graphics::Shader::Create(
              context, pair.second->path, pair.second->name,
              &pair.second->externs);

          if (Error::IsError(shaderModule)) {
            std::lock_guard<std::mutex> lock(StatusMutex);
            CompilationStatuses[pair.first] = ShaderCompilationStatus::Unloaded;
            CreatedCV.notify_all();
            return shaderModule.error();
          }

          {
            std::lock_guard<std::mutex> lock(StatusMutex);

            LoadedShaders[pair.first] = shaderModule.value();
            CompilationStatuses.at(pair.first) =
                ShaderCompilationStatus::Created;

            CreatedCV.notify_all();
          }

          return {};
        });

    if (Error::IsError(error)) {
      PrintError(error.ToString());
    }
  });

  compiler.detach();

  return {};
}

auto ShaderManager::ReloadShaders() -> void {
  std::lock_guard<std::mutex> lock(StatusMutex);

  LoadedShaders.clear();
  for (auto &[shaderKey, status] : CompilationStatuses) {
    (void)shaderKey;
    status = ShaderCompilationStatus::Unloaded;
  }

  CreatedCV.notify_all();
}

} // namespace Engine::Renderer