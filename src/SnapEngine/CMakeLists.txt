add_library(SnapEngine
  material.cpp
  model.cpp
  "project.cpp"
  scene.cpp
  gltfLoader.cpp

  Editor/gui.cpp

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
)

message(STATUS "SnapEngine include: ${PROJECT_SOURCE_DIR}/src/Snap")

target_link_libraries(SnapEngine
  PUBLIC snap_core
)