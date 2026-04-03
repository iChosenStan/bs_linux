cmake_minimum_required(VERSION 3.16)
project(BloodStrikeLinux VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Vulkan REQUIRED)

# X11 and GLFW
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(XINPUT REQUIRED xi)
pkg_check_modules(GLFW REQUIRED glfw3)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/imgui
    ${CMAKE_SOURCE_DIR}/libs/imgui/backends
    ${Vulkan_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${XINPUT_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
)

# ImGui source files
set(IMGUI_SOURCES
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/backends/imgui_impl_glfw.cpp
    libs/imgui/backends/imgui_impl_vulkan.cpp
)

# Main source files
set(MAIN_SOURCES
    src/main.cpp
    src/menu.cpp
    src/cheat_features.cpp
    src/renderer.cpp
    src/input.cpp
    src/injector.cpp
    src/hooks.cpp
    src/pixelbot.cpp
    src/entity_manager.cpp
    src/aimbot.cpp
    src/esp.cpp
    src/skin_changer.cpp
    src/offset_scanner.cpp
)

# Optional: OpenCV for YOLO AI aimbot (uncomment if using)
# find_package(OpenCV REQUIRED)
# add_definitions(-DUSE_OPENCV)
# set(MAIN_SOURCES ${MAIN_SOURCES} src/yolo_detector.cpp)

#=============================================================================
# Shared Library (Cheat Module)
#=============================================================================
add_library(bloodstrike_linux SHARED ${MAIN_SOURCES} ${IMGUI_SOURCES})

target_link_libraries(bloodstrike_linux
    ${Vulkan_LIBRARY}
    ${X11_LIBRARIES}
    ${XINPUT_LIBRARIES}
    ${GLFW_LIBRARIES}
    dl
    pthread
)

set_target_properties(bloodstrike_linux PROPERTIES
    OUTPUT_NAME "bloodstrike"
    PREFIX "lib"
    SUFFIX ".so"
)

#=============================================================================
# Standalone Injector Executable (CLI)
#=============================================================================
add_executable(bloodstrike_injector
    src/injector_main.cpp
    src/stealth_injector.cpp
)

target_link_libraries(bloodstrike_injector
    dl
    pthread
)

set_target_properties(bloodstrike_injector PROPERTIES
    OUTPUT_NAME "bloodstrike-injector"
)

#=============================================================================
# GUI Loader
#=============================================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

add_executable(bloodstrike_loader
    src/loader_gui.cpp
    src/stealth_injector.cpp
    src/offset_scanner.cpp
)

target_include_directories(bloodstrike_loader PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_libraries(bloodstrike_loader
    ${GTK3_LIBRARIES}
    dl
    pthread
)

set_target_properties(bloodstrike_loader PROPERTIES
    OUTPUT_NAME "bloodstrike-loader"
)

#=============================================================================
# Install Targets
#=============================================================================
install(TARGETS bloodstrike_linux LIBRARY DESTINATION lib)
install(TARGETS bloodstrike_injector RUNTIME DESTINATION bin)

#=============================================================================
# Build Configuration
#=============================================================================
# Strip release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET bloodstrike_linux POST_BUILD
        COMMAND strip -s $<TARGET_FILE:bloodstrike_linux>
    )
    add_custom_command(TARGET bloodstrike_injector POST_BUILD
        COMMAND strip -s $<TARGET_FILE:bloodstrike_injector>
    )
endif()