#include "Graphics/buffer.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/bytedata.hpp"
#include "Wrap/wrap.hpp"
#include <lua.h>

namespace Graphics {

// Bytedata, [offset], [range]
auto Wrap_SetVertices(lua_State *state) -> int {
  auto *mesh = LuaWrap::FromLuaObject<Mesh>(state, 1);
  auto *data = LuaWrap::FromLuaObject<Data::ByteData>(state, 2);

  auto offset = 0L;
  if (lua_gettop(state) >= 3) {
    offset = static_cast<int64_t>(luaL_checkinteger(state, 3));
  }

  if (offset >= data->GetSize() || offset < 0) {
    return luaL_error(state, "Vertex data offset out of bounds.");
  }

  auto count = data->GetSize() - offset;
  if (lua_gettop(state) >= 4) {
    count = static_cast<int64_t>(luaL_checkinteger(state, 4));

    if (count < 0) {
      return luaL_error(state, "Vertex data count cannot be negative.");
    }
  }

  if (offset + count > data->GetSize()) {
    return luaL_error(state, "Vertex data range out of bounds.");
  }

  std::span<uint8_t> vertexData = // NOLINTNEXTLINE, pointer arithmetic
      std::span<uint8_t>(data->GetData() + offset, static_cast<size_t>(count));

  auto result = mesh->SetVertices(*Graphics::GetCurrentGraphicsContext(),
                                  vertexData, static_cast<uint64_t>(offset));

  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.ToString().c_str());
  }

  return 0;
}
auto Wrap_SetIndices(lua_State *state) -> int {
  auto *mesh = LuaWrap::FromLuaObject<Mesh>(state, 1);
  auto *data = LuaWrap::FromLuaObject<Data::ByteData>(state, 2);

  auto offset = 0L;
  if (lua_gettop(state) >= 3) {
    offset = static_cast<int64_t>(luaL_checkinteger(state, 3));
  }

  if (offset >= data->GetSize() || offset < 0) {
    return luaL_error(state, "Index data offset out of bounds.");
  }

  auto count = data->GetSize() - offset;
  if (lua_gettop(state) >= 4) {
    count = static_cast<int64_t>(luaL_checkinteger(state, 4));

    if (count < 0) {
      return luaL_error(state, "Index data count cannot be negative.");
    }
  }

  if (offset + count > data->GetSize()) {
    return luaL_error(state, "Index data range out of bounds.");
  }

  std::span<uint32_t> indexData = std::span<
      uint32_t>( // NOLINTNEXTLINE, pointer arithmetic and reinterpret cast
      reinterpret_cast<uint32_t *>(data->GetData() + offset),
      static_cast<size_t>(count / sizeof(uint32_t)));

  auto result = mesh->SetIndices(*Graphics::GetCurrentGraphicsContext(),
                                 indexData, static_cast<uint64_t>(offset));

  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.ToString().c_str());
  }

  return 0;
}

auto Wrap_SetVertexBuffer(lua_State *state) -> int {
  auto *mesh = LuaWrap::FromLuaObject<Mesh>(state, 1);
  auto *buffer = LuaWrap::FromLuaObject<Buffer>(state, 2);

  mesh->SetVertexBuffer(Ref<Buffer>(buffer));

  return 0;
}
auto Wrap_SetIndexBuffer(lua_State *state) -> int {
  auto *mesh = LuaWrap::FromLuaObject<Mesh>(state, 1);
  auto *buffer = LuaWrap::FromLuaObject<Buffer>(state, 2);

  mesh->SetIndexBuffer(Ref<Buffer>(buffer));

  return 0;
}

auto Wrap_SetDrawRange(lua_State *state) -> int {
  auto *mesh = LuaWrap::FromLuaObject<Mesh>(state, 1);
  auto offset = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  auto count = static_cast<uint32_t>(luaL_checkinteger(state, 3));

  mesh->SetDrawRange(MeshDrawRange{.Offset = offset, .Count = count});

  return 0;
}
auto Wrap_GetDrawRange(lua_State *state) -> int {
  auto *mesh = LuaWrap::FromLuaObject<Mesh>(state, 1);
  auto range = mesh->GetDrawRange();

  lua_pushinteger(state, static_cast<lua_Integer>(range.Offset));
  lua_pushinteger(state, static_cast<lua_Integer>(range.Count));

  return 2;
}

auto Wrap_NewMesh(lua_State *state) -> int {
  // TODO: Rework meshes to allow for custom vertex formats //

  return luaL_error(state, "Wrap_NewMesh not implemented.");

  // auto *ctx = Graphics::GetCurrentGraphicsContext();
  // auto vertexFormat =
  //     static_cast<VertexFormats>(luaL_checkinteger(state, 1));
  // auto *vertexData = LuaWrap::FromLuaObject<Data::ByteData>(state, 2);

  // std::vector<uint32_t> *indexData = nullptr;
  // if (lua_gettop(state) >= 3 && !lua_isnil(state, 3)) {
  //   auto *indexDataObj = LuaWrap::FromLuaObject<Data::ByteData>(state, 3);
  //   size_t indexCount = indexDataObj->GetSize() / sizeof(uint32_t);
  //   indexData = new std::vector<uint32_t>(indexCount);
  //   // NOLINTNEXTLINE; Reinterpret cast is necessary here
  //   std::memcpy(indexData->data(), indexDataObj->GetData(),
  //               indexDataObj->GetSize());
  // }

  // std::span<uint8_t> vertexSpan = // NOLINTNEXTLINE, pointer arithmetic
  //     std::span<uint8_t>(vertexData->GetData(),
  //                        static_cast<size_t>(vertexData->GetSize()));

  // auto result =
  //     Mesh::Create(*ctx, vertexFormat, vertexSpan, indexData);

  // if (indexData != nullptr) {
  //   delete indexData;
  // }

  // if (Error::IsError(result)) {
  //   return luaL_error(state, "%s", result.error().ToString().c_str());
  // }

  // LuaWrap::ToLuaObject<Mesh>(state, result.value());
  // return 1;
}

auto Wrap_Release(lua_State *state) -> int {
  auto *mesh = LuaWrap::FromLuaObject<Mesh>(state, 1);
  mesh->ScheduleDestroy();
  return 0;
}
} // namespace Graphics