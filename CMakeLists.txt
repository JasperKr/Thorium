cmake_minimum_required(VERSION 3.25)
project(snap LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/std:c++23preview>")
else()
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# -------------------------------
# Find Vulkan first
# -------------------------------
find_package(Vulkan REQUIRED)

if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found! Make sure VULKAN_SDK is set.")
endif()

# -------------------------------
# SDL3
# -------------------------------
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

# -------------------------------
# ImGui
# -------------------------------
add_library(imgui SHARED IMPORTED)
set(CIMGUI_LIB "${CMAKE_CURRENT_SOURCE_DIR}/bin/cimgui/cimgui.so")
set_target_properties(imgui PROPERTIES
    IMPORTED_LOCATION "${CIMGUI_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui"
)
target_link_libraries(imgui INTERFACE SDL3::SDL3 Vulkan::Vulkan)

# -------------------------------
# PhysFS
# -------------------------------

set(PHYSFS_SOURCES
    external/physfs/src/physfs.c
    external/physfs/src/physfs_byteorder.c
    external/physfs/src/physfs_unicode.c
    external/physfs/src/physfs_archiver_dir.c
    external/physfs/src/physfs_archiver_unpacked.c
    external/physfs/src/physfs_archiver_grp.c
    external/physfs/src/physfs_archiver_hog.c
    external/physfs/src/physfs_archiver_7z.c
    external/physfs/src/physfs_archiver_mvl.c
    external/physfs/src/physfs_archiver_qpak.c
    external/physfs/src/physfs_archiver_wad.c
    external/physfs/src/physfs_archiver_zip.c
    external/physfs/src/physfs_archiver_slb.c
    external/physfs/src/physfs_archiver_iso9660.c
    external/physfs/src/physfs_archiver_csm.c
    external/physfs/src/physfs_archiver_vdf.c
    external/physfs/src/physfs_archiver_lec3d.c
)

if (WIN32)
    message(STATUS "Configuring for Windows")
    list(APPEND PHYSFS_SOURCES
        external/physfs/src/physfs_platform_windows.c
    )
elseif(UNIX AND NOT APPLE)
    message(STATUS "Configuring for Unix")
    list(APPEND PHYSFS_SOURCES
        external/physfs/src/physfs_platform_unix.c
        external/physfs/src/physfs_platform_posix.c
    )
elseif(APPLE)
    message(STATUS "Configuring for Apple")
    list(APPEND PHYSFS_SOURCES
        external/physfs/src/physfs_platform_apple.c
    )
endif()

add_library(physfs STATIC ${PHYSFS_SOURCES})
target_include_directories(physfs
    SYSTEM PRIVATE external/physfs/src/
)

target_compile_definitions(physfs PRIVATE _POSIX_C_SOURCE=200809L)

set(BINARIES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin/")
set(LIBRARIES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/")
set(SHADERC_BIN_DIR "${BINARIES_DIR}/shaderc/")
set(SHADERC_LIB_DIR "${LIBRARIES_DIR}/shaderc/")

# -------------------------------
# Shaderc
# -------------------------------

# Shaderc library
if(WIN32)
    set(SHADERC_LIB "${SHADERC_LIB_DIR}/shaderc_shared.lib")
elseif(UNIX)
    set(SHADERC_LIB "${SHADERC_LIB_DIR}/libshaderc_shared.so")
endif()

# -------------------------------
# LuaJIT
# -------------------------------

include(ExternalProject)

set(LUAJIT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/external/luajit/")
make_directory(${CMAKE_SOURCE_DIR}/thirdparty/luajit)
file(COPY ${CMAKE_SOURCE_DIR}/thirdparty/luajit DESTINATION ${LUAJIT_SRC})

if (WIN32)
    ExternalProject_Add(luajit
        SOURCE_DIR ${LUAJIT_SRC}/src
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            cmd /c "cd ${LUAJIT_SRC}/src && msvcbuild.bat amalg"
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE 1
    )

    # Define imported target
    add_library(LuaJIT::LuaJIT SHARED IMPORTED)

    set_target_properties(LuaJIT::LuaJIT PROPERTIES
        IMPORTED_LOCATION     "${LUAJIT_SRC}/src/lua51.dll"
        IMPORTED_IMPLIB       "${LUAJIT_SRC}/src/lua51.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_SRC}/src"
    )
elseif(UNIX)
    ExternalProject_Add(luajit
        SOURCE_DIR ${LUAJIT_SRC}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            make -C ${LUAJIT_SRC}/src
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE 1
        BUILD_BYPRODUCTS ${LUAJIT_SRC}/src/libluajit.so
    )

    add_library(LuaJIT::LuaJIT SHARED IMPORTED)

    set_target_properties(LuaJIT::LuaJIT PROPERTIES
        IMPORTED_LOCATION "${LUAJIT_SRC}/src/libluajit.so"
        INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_SRC}/src"
    )
endif()


# Ensure snap depends on LuaJIT build
add_dependencies(LuaJIT::LuaJIT luajit)

# -------------------------------
# FastGLTF
# -------------------------------

set(fastGLTFSources
    include/fastgltf/src/fastgltf.cpp
    include/fastgltf/src/base64.cpp
    include/fastgltf/src/io.cpp
)

add_library(fastgltf STATIC ${fastGLTFSources})

target_include_directories(fastgltf
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/fastgltf/include
)

add_library(simdjson STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include/simdjson/simdjson.cpp
)

target_include_directories(simdjson
    PUBLIC include
)

target_link_libraries(fastgltf
    PUBLIC simdjson
)

# -------------------------------
# Tracy
# -------------------------------

find_package(Threads REQUIRED)
add_library(tracy STATIC ${CMAKE_CURRENT_SOURCE_DIR}/external/tracy/public/TracyClient.cpp)

target_include_directories(tracy
    SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/tracy
)

target_compile_definitions(tracy
    PUBLIC TRACY_ENABLE
)

target_link_libraries(tracy
    PUBLIC Threads::Threads
)


# -------------------------------
# Slang
# -------------------------------

set(SLANG_BIN_DIR "${BINARIES_DIR}/slang/")
set(SLANG_LIB_DIR "${LIBRARIES_DIR}/slang/")

if(WIN32)
    set(SLANG_LIBS
        ${SLANG_LIB_DIR}/slang.lib
        ${SLANG_LIB_DIR}/gfx.lib
        ${SLANG_LIB_DIR}/slang-compiler.lib
        ${SLANG_LIB_DIR}/slang-rt.lib
        )
elseif(UNIX)
    # set(SLANG_LIBS "${SLANG_LIB_DIR}/libgfx.so.0.2025.23.1")
    # Find all .so files in the directory
    file(GLOB SO_FILES "${SLANG_LIB_DIR}/*.so*")

    # Add them to the variable
    set(SLANG_LIBS ${SO_FILES})

endif()

# -------------------------------
# Engine core (shared object library)
# -------------------------------
add_library(snap_core OBJECT
# Graphics module
    src/Snap/Graphics/graphics.cpp
    src/Snap/Graphics/texture.cpp
    src/Snap/Graphics/buffer.cpp
    src/Snap/Graphics/render.cpp
    src/Snap/Graphics/shader.cpp
    src/Snap/Graphics/rendergraph.cpp
    src/Snap/Graphics/sampler.cpp
    src/Snap/Graphics/dynamicRendering.cpp
    src/Snap/Graphics/reflect.cpp
    src/Snap/Graphics/resource.cpp
    src/Snap/Graphics/mesh.cpp
    src/Snap/Graphics/draw.cpp
    src/Snap/Graphics/barrier.cpp
    src/Snap/Graphics/renderThread.cpp
    src/Snap/Graphics/semaphoreManager.cpp
    src/Snap/Graphics/bufferformat.cpp
    src/Snap/Graphics/swapchainManager.cpp

# Graphics Buffers
    src/Snap/Graphics/Buffers/push.cpp
    src/Snap/Graphics/Buffers/uniform.cpp
    src/Snap/Graphics/Buffers/structured.cpp

# Engine Modules
    src/Snap/Modules/filesystem.cpp
    src/Snap/Modules/timer.cpp
    src/Snap/Modules/errorhandler.cpp
    src/Snap/Modules/config.cpp
    src/Snap/Modules/event.cpp
    src/Snap/Modules/object.cpp
    src/Snap/Modules/imagedata.cpp
    src/Snap/Modules/Peripherals/mouse.cpp
    src/Snap/Modules/Peripherals/keyboard.cpp
    src/Snap/Modules/thread.cpp
    src/Snap/Modules/channel.cpp
    src/Snap/Modules/utils.cpp
    src/Snap/Modules/error.cpp
    src/Snap/Modules/window.cpp
    src/Snap/Modules/reflectBindings.cpp

# Math Modules
    src/Snap/Modules/Math/vector.cpp
    src/Snap/Modules/Math/matrix.cpp
    src/Snap/Modules/Math/quaternion.cpp
    src/Snap/Modules/Math/math.cpp

# Wrappers
    src/Snap/Wrap/Graphics/wrap_graphics.cpp
    src/Snap/Wrap/Graphics/wrap_texture.cpp
    src/Snap/Wrap/Graphics/wrap_rendertarget.cpp
    src/Snap/Wrap/Graphics/wrap_mesh.cpp
    src/Snap/Wrap/Graphics/wrap_shader.cpp
    src/Snap/Wrap/Graphics/wrap_buffer.cpp
    src/Snap/Wrap/Graphics/wrap_format.cpp
    src/Snap/Wrap/Modules/wrap_event.cpp
    src/Snap/Wrap/Modules/wrap_timer.cpp
    src/Snap/Wrap/Modules/wrap_data.cpp
    src/Snap/Wrap/Modules/wrap_bytedata.cpp
    src/Snap/Wrap/Modules/wrap_imagedata.cpp
    src/Snap/Wrap/Modules/wrap_filesystem.cpp
    src/Snap/Wrap/Modules/wrap_keyboard.cpp
    src/Snap/Wrap/Modules/wrap_mouse.cpp
    src/Snap/Wrap/Modules/wrap_thread.cpp
    src/Snap/Wrap/Modules/wrap_channel.cpp
    src/Snap/Wrap/Modules/wrap_window.cpp
    src/Snap/Wrap/Modules/wrap_math.cpp
    src/Snap/Wrap/wrap.cpp
    src/Snap/Wrap/lua_data.cpp

# Libraries
    src/Snap/Libraries/vma.cpp
    src/Snap/Libraries/load_stb.cpp
    src/Snap/Libraries/load_volk.cpp
)

target_include_directories(snap_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Snap
)
target_include_directories(snap_core SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/physfs/src
        ${Vulkan_INCLUDE_DIRS}
)

target_compile_options(snap_core PRIVATE -fno-exceptions -fno-rtti)
target_compile_definitions(snap_core
    PUBLIC VK_NO_PROTOTYPES
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_ENABLE_FREETYPE
    PRIVATE
        IMGUI_IMPL_VULKAN_NO_PROTOTYPES
        IMGUI_IMPL_VULKAN_USE_LOADER
)

target_link_libraries(snap_core
    PUBLIC
        imgui
        SDL3::SDL3
        Vulkan::Vulkan
        physfs
        ${SHADERC_LIB}
        ${Vulkan_LIBRARIES}
        LuaJIT::LuaJIT
        ${SLANG_LIBS}
        fastgltf
        tracy
)

# -------------------------------
# Engine executable
# -------------------------------
add_executable(snap
    src/Snap/main.cpp
    src/Snap/loop.cpp
    $<TARGET_OBJECTS:snap_core>
)

target_include_directories(snap
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/SnapEngine
)

# Copy LuaJIT DLL after build
if (WIN32)
    add_custom_command(TARGET snap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LUAJIT_SRC}/src/lua51.dll"
            $<TARGET_FILE_DIR:snap>)
endif()

add_subdirectory(src/SnapEngine)

# Link libraries
target_link_libraries(snap
    PRIVATE
        snap_core
        SnapEngine
)

target_link_options(snap PRIVATE
    $<$<CONFIG:Debug>:-rdynamic>
)

if (WIN32)
    target_link_libraries(snap PRIVATE dbghelp)
endif()

# -------------------------------
# Windows: copy SDL3.dll after build
# -------------------------------
if (WIN32)
    add_custom_command(TARGET snap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:snap>
    )
endif()

# -------------------------------
# Compiler warnings & options
# -------------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wdouble-promotion
        -Werror
        -fno-exceptions
        -fno-rtti
    )
elseif (MSVC)
    add_compile_options(
        /W4 /WX /permissive-
    )
endif()

add_executable(lua_stub_gen
    src/Snap/generateBindings.cpp
    $<TARGET_OBJECTS:snap_core>
    $<TARGET_OBJECTS:SnapEngine>
)

target_compile_definitions(lua_stub_gen
  PRIVATE
    LUA_STUB_GEN
)

target_link_libraries(lua_stub_gen
    PRIVATE
        snap_core
        SnapEngine
)

add_custom_target(generate_bindings
    COMMAND lua_stub_gen
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating Lua bindings..."
)

