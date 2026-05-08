add_library(SnapEngine
  material.cpp
  gltfLoader.cpp
  renderer.cpp
  style.cpp
  frustum.cpp

  Editor/gui.cpp

  Scene/scene.cpp
  Scene/project.cpp
  Scene/transform.cpp
  Scene/userdata.cpp

  Scene/Geometry/shape.cpp
  Scene/Geometry/levelOfDetail.cpp
  Scene/Geometry/boundingBox.cpp
  Scene/Geometry/geometry.cpp
  Scene/Geometry/model.cpp

  Scene/Lights/directionalLight.cpp
  Scene/Lights/pointLight.cpp
  Scene/Lights/spotLight.cpp
  Scene/Lights/rectangleLight.cpp
  Scene/Lights/sphereLight.cpp

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