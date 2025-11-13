cmake_minimum_required(VERSION 3.20)
project(Thorium LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

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
add_library(imgui
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_sdl3.cpp
    external/imgui/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC external/imgui)
target_link_libraries(imgui PUBLIC SDL3::SDL3 Vulkan::Vulkan)

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

# -------------------------------
# Shaderc
# -------------------------------

# Shaderc library
if(WIN32)
    set(SHADERC_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib/")
    set(SHADERC_LIB "${SHADERC_LIB_DIR}/shaderc_shared.lib")
elseif(UNIX)
    set(SHADERC_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib/")
    set(SHADERC_LIB "${SHADERC_LIB_DIR}/libshaderc_shared.so")
endif()

# -------------------------------
# Engine executable
# -------------------------------
add_executable(Thorium
    src/main.cpp
    src/program.cpp

    src/Graphics/graphics.cpp
    src/Graphics/texture.cpp
    src/Graphics/buffer.cpp
    src/Graphics/render.cpp
    src/Graphics/shader.cpp
    src/Graphics/rendergraph.cpp

    src/Modules/filesystem.cpp
    src/Modules/timer.cpp

    src/Libraries/vma.cpp
    src/Libraries/load_stb.cpp
    src/Libraries/load_volk.cpp
)

# Include paths
target_include_directories(Thorium
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    SYSTEM PRIVATE
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
        ${Vulkan_LIBRARIES}       # usually optional
)

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
