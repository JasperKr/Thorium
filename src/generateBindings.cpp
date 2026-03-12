#include "Modules/reflectBindings.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
auto main() -> int {
  auto *state = luaL_newstate();
  std::cout << "Generating Lua bindings...\n";
  LuaWrap::RegisterModules(state);

  // Log real path
  std::cout << "Current working directory: "
            << std::filesystem::current_path().string() << "\n";

  std::cout << "Emitting Lua bindings to 'generated/' directory...\n";
  std::filesystem::create_directories("generated");
  for (const auto &module : Bindings::LuaModules) {
    std::string filename = "generated/" + module.first + "_bindings.d.lua";
    std::cout << "Generating bindings for module: " << module.first << " -> "
              << filename << "\n";
    std::ofstream outFile(filename);

    if (!outFile) {
      std::cerr << "Failed to open file for module: " << module.first << "\n";
      continue;
    }

    Bindings::EmitLuaStruct(outFile, module.first, module.second);
    outFile.close();
  }

  std::cout << "Emitting Lua enums to 'generated/lua_enums.d.lua'...\n";
  std::ofstream enumOutFile("generated/lua_enums.d.lua");
  if (!enumOutFile) {
    std::cerr << "Failed to open file for Lua enums\n";
    return 1;
  }

  Bindings::EmitLuaEnums(enumOutFile, LuaWrap::RegisteredEnums);
  enumOutFile.close();

  lua_close(state);
  std::cout << "Lua bindings generation completed.\n";
}