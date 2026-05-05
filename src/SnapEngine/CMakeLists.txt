add_library(SnapEngine
  material.cpp
  gltfLoader.cpp
  renderer.cpp
  style.cpp
  Editor/gui.cpp

  Scene/scene.cpp
  Scene/project.cpp
  Scene/model.cpp
  Scene/shape.cpp
  Scene/transform.cpp
  Scene/levelOfDetail.cpp
  Scene/boundingBox.cpp
  Scene/userdata.cpp
  Scene/geometry.cpp

  # Wrappers
  Wrap/wrap_scene.cpp
  Wrap/wrap_renderer.cpp
  Wrap/wrap_imgui.cpp
  Wrap/wrap_engine.cpp
)

target_include_directories(SnapEngine
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src/Snap
    ${PROJECT_SOURCE_DIR}/src/SnapEngine
    ${PROJECT_SOURCE_DIR}/include/flecs
)

message(STATUS "SnapEngine include: ${PROJECT_SOURCE_DIR}/src/Snap")

target_link_libraries(SnapEngine
  PUBLIC snap_core
)