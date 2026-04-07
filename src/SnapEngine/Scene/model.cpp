#include "model.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/console.hpp"
#include "Modules/object.hpp"
#include "Scene/scene.hpp"
#include "Scene/shape.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "material.hpp"
#include <lua.h>

namespace Engine {
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

// world:createModel("model", { x, y, z }, { rotX, rotY, rotZ, rotW }, { scaleX, scaleY, scaleZ }, { shape1, shape2 }, material)
auto LuaModel::Create(lua_State *state) -> int {
  auto *scene = LuaWrap::ObjectFromLua<Scene>(state, 1);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a Scene object");
  }

  const char *name = luaL_checkstring(state, 2);
  auto entity = scene->world.entity(name);

  entity.add<Model>();
  entity.add<Userdata>();

  Math::Vec3 position{0.0F, 0.0F, 0.0F};
  // Position table
  if (lua_gettop(state) >= 3) {
    luaL_checktype(state, 3, LUA_TTABLE);
    lua_rawgeti(state, 3, 1);
    position.x = static_cast<Math::Scalar>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 3, 2);
    position.y = static_cast<Math::Scalar>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 3, 3);
    position.z = static_cast<Math::Scalar>(luaL_checknumber(state, -1));
    lua_pop(state, 3);
  }

  // Rotation table
  Math::Quaternion rotation{0.0F, 0.0F, 0.0F, 1.0F};
  if (lua_gettop(state) >= 4) {
    luaL_checktype(state, 4, LUA_TTABLE);
    lua_rawgeti(state, 4, 1);
    rotation.x = static_cast<Math::Scalar>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 4, 2);
    rotation.y = static_cast<Math::Scalar>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 4, 3);
    rotation.z = static_cast<Math::Scalar>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 4, 4);
    rotation.w = static_cast<Math::Scalar>(luaL_checknumber(state, -1));
    lua_pop(state, 4);
  }

  // Scale table
  Math::Vec3 scale{1.0F, 1.0F, 1.0F};
  if (lua_gettop(state) >= 5) {
    luaL_checktype(state, 5, LUA_TTABLE);
    lua_rawgeti(state, 5, 1);
    scale.x = static_cast<Math::Scalar>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 5, 2);
    scale.y = static_cast<Math::Scalar>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 5, 3);
    scale.z = static_cast<Math::Scalar>(luaL_checknumber(state, -1));
    lua_pop(state, 3);
  }

  entity.set<Transform>(Transform{
      .Position = position,
      .Rotation = rotation,
      .Scale = scale,
  });

  if (lua_gettop(state) >= 6) {
    luaL_checktype(state, 6, LUA_TTABLE);
    lua_pushnil(state);
    while (lua_next(state, 6) != 0) {
      auto *luaShape = LuaWrap::ObjectFromLua<LuaShape>(state, -1);
      if (luaShape != nullptr) {
        luaShape->entity.child_of(entity);
      } else {
        return luaL_error(state, "Expected a Shape object in the shapes table");
      }
      lua_pop(state, 1);
    }
  }

  if (lua_gettop(state) >= 7) {
    auto *luaMaterial = LuaWrap::ObjectFromLua<Renderer::LuaMaterial>(state, 7);
    if (luaMaterial != nullptr) {
      entity.set<Renderer::Material>(luaMaterial->material);
    }
  }

  auto model = Ref<LuaModel>::Make(entity);
  LuaWrap::PushObject(state, LuaModel::GetType(), model.get());

  return 1;
}

auto LuaModel::GetName(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<LuaModel>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Expected a Model object");
  }

  lua_pushstring(state, model->entity.name());
  return 1;
}

auto LuaModel::SetName(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<LuaModel>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Expected a Model object");
  }

  const char *newName = luaL_checkstring(state, 2);
  model->entity.set_name(newName);
  return 0;
}

auto LuaModel::GetPosition(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<LuaModel>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Expected a Model object");
  }

  auto transform = model->entity.get_ref<Transform>();
  if (transform.get() == nullptr) {
    return luaL_error(state, "Transform component not found");
  }

  lua_pushnumber(state, transform->Position.x);
  lua_pushnumber(state, transform->Position.y);
  lua_pushnumber(state, transform->Position.z);

  return 3;
}

auto LuaModel::SetPosition(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<LuaModel>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Expected a Model object");
  }

  auto *transform = model->entity.try_get_mut<Transform>();
  if (transform == nullptr) {
    return luaL_error(state, "Transform component not found");
  }

  transform->Position.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  transform->Position.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  transform->Position.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));

  return 0;
}

auto LuaModel::GetRotation(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<LuaModel>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Expected a Model object");
  }

  auto transform = model->entity.get_ref<Transform>();
  if (transform.get() == nullptr) {
    return luaL_error(state, "Transform component not found");
  }

  lua_pushnumber(state, transform->Rotation.x);
  lua_pushnumber(state, transform->Rotation.y);
  lua_pushnumber(state, transform->Rotation.z);
  lua_pushnumber(state, transform->Rotation.w);

  return 4;
}

auto LuaModel::SetRotation(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<LuaModel>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Expected a Model object");
  }

  auto *transform = model->entity.try_get_mut<Transform>();
  if (transform == nullptr) {
    return luaL_error(state, "Transform component not found");
  }

  transform->Rotation.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  transform->Rotation.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  transform->Rotation.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));
  transform->Rotation.w = static_cast<Math::Scalar>(luaL_checknumber(state, 5));

  return 0;
}

auto LuaModel::GetScale(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<LuaModel>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Expected a Model object");
  }

  auto transform = model->entity.get_ref<Transform>();
  if (transform.get() == nullptr) {
    return luaL_error(state, "Transform component not found");
  }

  lua_pushnumber(state, transform->Scale.x);
  lua_pushnumber(state, transform->Scale.y);
  lua_pushnumber(state, transform->Scale.z);

  return 3;
}

auto LuaModel::SetScale(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<LuaModel>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Expected a Model object");
  }

  auto *transform = model->entity.try_get_mut<Transform>();
  if (transform == nullptr) {
    return luaL_error(state, "Transform component not found");
  }

  transform->Scale.x = static_cast<Math::Scalar>(luaL_checknumber(state, 2));
  transform->Scale.y = static_cast<Math::Scalar>(luaL_checknumber(state, 3));
  transform->Scale.z = static_cast<Math::Scalar>(luaL_checknumber(state, 4));

  return 0;
}

auto LuaModel::GetShapes(lua_State *state) -> int {
  auto *model = LuaWrap::ObjectFromLua<LuaModel>(state, 1);
  if (model == nullptr) {
    return luaL_error(state, "Expected a Model object");
  }

  if (lua_gettop(state) == 1) {
    lua_newtable(state);
  } else {
    luaL_checktype(state, 2, LUA_TTABLE);
  }

  int index = 1;
  model->entity.children<Shape>([&](flecs::entity entity) -> void {
    lua_pushinteger(state, index++);
    auto shapeObject = LuaShape::FromEntity(entity);
    LuaWrap::PushObject(state, LuaShape::GetType(), shapeObject.get());
    lua_settable(state, -3);
  });

  return 1;
}

const LuaWrap::LuaClass ModelClass = {
    .Name = "Model",
    .Type = LuaModel::GetType(),
    .Methods =
        {
            {"getName", LuaModel::GetName},
            {"setName", LuaModel::SetName},
            {"getPosition", LuaModel::GetPosition},
            {"setPosition", LuaModel::SetPosition},
            {"getRotation", LuaModel::GetRotation},
            {"setRotation", LuaModel::SetRotation},
            {"getScale", LuaModel::GetScale},
            {"setScale", LuaModel::SetScale},
            {"getShapes", LuaModel::GetShapes},
        },
};

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
} // namespace Engine