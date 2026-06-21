#include "lightProbe.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "renderer.hpp"

namespace Engine::Renderer {

auto LightProbe::Render(const Transform &transform) -> Error {
  auto *ctx = Graphics::GetCurrentGraphicsContext();

  CHECK_ERR(RendererInstance.GetPrefilterManager().PrefilterLightProbe(
      *ctx, *this, scene, transform));

  return {};
}

auto LuaLightProbe::Create(lua_State *state) -> int {
  auto *scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));
  auto entity = scene->world.entity();

  entity.set(LightProbe{.scene = scene});
  entity.add<Transform>();
  entity.set<DisplayName>({luaL_optstring(state, 2, "LightProbe")});
  entity.add<Userdata>();

  auto luaProbe = Ref<LuaLightProbe>::Make(entity);

  ::LuaWrap::PushObject(state, LuaLightProbe::GetType(), luaProbe.get());
  return 1;
}

auto LuaLightProbe::Render(lua_State *state) -> int {
  auto *obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaLightProbe>(state, 1));
  auto *lightProbe = LUA_CK_NULL(obj->entity.try_get_mut<LightProbe>());
  const auto *transform = LUA_CK_NULL(obj->entity.try_get<Transform>());

  LUA_CK_ERR(lightProbe->Render(*transform));

  return 0;
}

auto GetLuaLightProbeClass() -> ::LuaWrap::LuaClass {
  const ::LuaWrap::LuaClass LuaCameraClass = {
      .Name = "LightProbe",
      .Type = LuaLightProbe::GetType(),
      .Methods =
          {
              {"render", LuaLightProbe::Render},
          },
      .Components = {
          TransformComponent,
          DisplayNameComponent,
          UserdataComponent,
      }};

  return LuaCameraClass;
}

} // namespace Engine::Renderer
