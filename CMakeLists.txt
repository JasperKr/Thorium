cmake_minimum_required(VERSION 3.25)
project(Thorium LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)

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


# Ensure Thorium depends on LuaJIT build
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
# Engine executable
# -------------------------------
add_executable(Thorium
# Main source files
    src/main.cpp
    src/loop.cpp

# Graphics module
    src/Graphics/graphics.cpp
    src/Graphics/texture.cpp
    src/Graphics/buffer.cpp
    src/Graphics/render.cpp
    src/Graphics/shader.cpp
    src/Graphics/rendergraph.cpp
    src/Graphics/sampler.cpp
    src/Graphics/dynamicRendering.cpp
    src/Graphics/reflect.cpp
    src/Graphics/resource.cpp
    src/Graphics/mesh.cpp
    src/Graphics/draw.cpp
    src/Graphics/barrier.cpp
    src/Graphics/gltfLoader.cpp
    src/Graphics/renderThread.cpp
    src/Graphics/semaphoreManager.cpp
    src/Graphics/bufferformat.cpp

# Graphics Buffers
    src/Graphics/Buffers/push.cpp
    src/Graphics/Buffers/uniform.cpp
    src/Graphics/Buffers/structured.cpp

# Engine Modules
    src/Modules/filesystem.cpp
    src/Modules/timer.cpp
    src/Modules/errorhandler.cpp
    src/Modules/config.cpp
    src/Modules/event.cpp
    src/Modules/object.cpp
    src/Modules/imagedata.cpp
    src/Modules/Peripherals/mouse.cpp
    src/Modules/Peripherals/keyboard.cpp
    src/Modules/thread.cpp
    src/Modules/channel.cpp
    src/Modules/utils.cpp
    src/Modules/error.cpp
    src/Modules/window.cpp

# Math Modules
    src/Modules/Math/vector.cpp
    src/Modules/Math/matrix.cpp
    src/Modules/Math/quaternion.cpp

# Editor
    src/Modules/Editor/gui.cpp

# Wrappers
    src/Wrap/Graphics/wrap_graphics.cpp
    src/Wrap/Graphics/wrap_texture.cpp
    src/Wrap/Graphics/wrap_rendertarget.cpp
    src/Wrap/Graphics/wrap_mesh.cpp
    src/Wrap/Graphics/wrap_shader.cpp
    src/Wrap/Graphics/wrap_buffer.cpp
    src/Wrap/Modules/wrap_event.cpp
    src/Wrap/Modules/wrap_timer.cpp
    src/Wrap/Modules/wrap_data.cpp
    src/Wrap/Modules/wrap_bytedata.cpp
    src/Wrap/Modules/wrap_imagedata.cpp
    src/Wrap/Modules/wrap_filesystem.cpp
    src/Wrap/Modules/wrap_keyboard.cpp
    src/Wrap/Modules/wrap_mouse.cpp
    src/Wrap/Modules/Editor/wrap_imgui.cpp
    src/Wrap/Modules/Editor/wrap_gui.cpp
    src/Wrap/Modules/wrap_thread.cpp
    src/Wrap/Modules/wrap_channel.cpp
    src/Wrap/Modules/wrap_window.cpp
    src/Wrap/wrap.cpp
    src/Wrap/lua_data.cpp

# Libraries
    src/Libraries/vma.cpp
    src/Libraries/load_stb.cpp
    src/Libraries/load_volk.cpp
)

# ImGui definitions
target_compile_definitions(Thorium PRIVATE
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    IMGUI_IMPL_VULKAN_USE_LOADER
)

# Copy LuaJIT DLL after build
if (WIN32)
    add_custom_command(TARGET Thorium POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LUAJIT_SRC}/src/lua51.dll"
            $<TARGET_FILE_DIR:Thorium>)
endif()

# Include paths
target_include_directories(Thorium
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    SYSTEM PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/physfs/src
        ${Vulkan_INCLUDE_DIRS}   # now valid
)

# Link libraries
target_link_libraries(Thorium
    PRIVATE
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

target_link_options(Thorium PRIVATE
    $<$<CONFIG:Debug>:-rdynamic>
)

target_compile_options(Thorium PRIVATE -fno-exceptions -fno-rtti)

if (WIN32)
    target_link_libraries(Thorium PRIVATE dbghelp)
endif()

# -------------------------------
# Windows: copy SDL3.dll after build
# -------------------------------
if (WIN32)
    add_custom_command(TARGET Thorium POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:Thorium>
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

# Debug/Release flags
add_compile_options(
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)
