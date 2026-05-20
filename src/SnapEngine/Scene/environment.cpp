#include "environment.hpp"
#include "Graphics/texture.hpp"
#include "Modules/bindings.hpp"
#include "Modules/object.hpp"
#include "Scene/displayName.hpp"
#include "Scene/scene.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"

namespace Engine {

auto LuaEnvironment::LoadBinding(lua_State *state) -> int {
  Bindings::LuaBoundStruct<LuaEnvironment> bindings("Environment");
  bindings.DocumentCustomMethod(Bindings::MethodInfo{
      .name = "getSkyboxTexture",
      .description = "Gets the skybox texture of this environment."});
  bindings.Register(state);

  return 0;
}

auto LuaEnvironment::GetSkyboxTexture(lua_State *state) -> int {
  auto *obj = ::LuaWrap::ObjectFromLua<LuaEnvironment>(state, 1);
  if (obj == nullptr) {
    return luaL_error(state, "Invalid Environment object");
  }

  const auto *environment = obj->entity.try_get<Environment>();
  if (environment == nullptr) {
    return luaL_error(state, "Environment component not found on entity");
  }

  const auto &skyboxTexture = environment->SkyboxTexture;
  if (skyboxTexture == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  ::LuaWrap::PushObject(state, Graphics::Texture::GetType(),
                        skyboxTexture.get());

  return 1;
}

auto LuaEnvironment::Create(lua_State *state) -> int {
  auto *scene = ::LuaWrap::ObjectFromLua<Scene>(state, 1);
  const char *name = luaL_checkstring(state, 2);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a Scene object");
  }

  const auto &texture = ::LuaWrap::ObjectFromLua<Graphics::Texture>(state, 3);
  if (texture == nullptr) {
    return luaL_error(state, "Expected a Texture object for the skybox");
  }

  auto environmentEntity = scene->world.entity();
  environmentEntity.set<Environment>(
      Environment{Ref<Graphics::Texture>(texture)});
  environmentEntity.add<Userdata>();
  environmentEntity.set<DisplayName>({name});

  auto environment = Ref<LuaEnvironment>::Make(environmentEntity);

  ::LuaWrap::PushObject(state, LuaEnvironment::GetType(), environment.get());

  return 1;
}

auto GetEnvironmentLuaClass() -> const ::LuaWrap::LuaClass & {
  static ::LuaWrap::LuaClass lclass = {.Name = "Environment",
                                       .Type = LuaEnvironment::GetType(),
                                       .Components = {
                                           DisplayNameComponent,
                                           UserdataComponent,

                                       }};

  return lclass;
}

} // namespace Engine