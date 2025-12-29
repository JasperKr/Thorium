#pragma once

#include "error.hpp"
#include <span>
#include <string_view>
#include <vector>
namespace Filesystem {
struct Config {
  std::string identity; // Application identity for save dirs
};

auto GetConfig() -> Config &;

auto Init(const std::string &orgDir) -> Error;
auto Deinit() -> Error;
auto ReadFile(const std::string &path) -> Result<std::vector<unsigned char>>;
auto ReadTextFile(const std::string &path) -> Result<std::string>;
auto AppendFile(const std::string &path, std::span<const uint8_t> data)
    -> Error;
auto AppendFile(const std::string &path, std::string_view data) -> Error;
auto WriteFile(const std::string &path, std::span<const uint8_t> data) -> Error;
auto WriteFile(const std::string &path, std::string_view data) -> Error;
auto FileExists(const std::string &path) -> bool;
auto GetFileModTime(const std::string &path) -> uint64_t;
auto AddToSearchPath(const std::string &path, bool appendToPath) -> Error;
auto RemoveFromSearchPath(const std::string &path) -> Error;
auto Mount(const std::string &path, const std::string &mountPoint,
           bool appendToPath) -> Error;
auto Unmount(const std::string &path) -> Error;
auto GetRealPath(const std::string &path) -> Result<std::string>;
auto ListFiles(const std::string &path) -> Result<std::vector<std::string>>;

auto GetErrorCode() -> uint32_t;
auto GetError() -> Error;

auto GetSaveDirectory() -> std::string;
auto GetSourceDirectory() -> std::string;

#ifdef CreateDirectory
#undef CreateDirectory
#endif

auto CreateDirectory(const std::string &path) -> Error;

} // namespace Filesystem

namespace Path {
// Returns the filename from a path
// /path/to/file.txt -> file.txt
auto Filename(const std::string &path) -> std::string;
// Returns the extension of the file
// file.txt -> txt
auto Extension(const std::string &path) -> std::string;
// Returns the directory from a path
// /path/to/file.txt -> /path/to/
auto Directory(const std::string &path) -> std::string;

auto Join(const std::string &base, const std::string &append) -> std::string;
template <typename... Strings>
auto Join(std::string_view first, const Strings &...rest) -> std::string;
auto Join(const std::vector<std::string> &paths) -> std::string;
auto Join(const char *base, const char *append) -> std::string;

auto Sanitize(const std::string &path) -> std::string;

} // namespace Path