cmake_minimum_required(VERSION 3.19)

project(gravity VERSION 0.1 LANGUAGES CXX)

include(cmake/general.cmake)

set(IMPORT_SDL_FROM "SYSTEM" CACHE STRING "Choose how to import sdl2: " FORCE)
set_property(CACHE IMPORT_SDL_FROM PROPERTY STRINGS "CONAN" "SYSTEM")

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                                 Conan                                  #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
set(CONAN SYSTEM_INCLUDES ON)
include(cmake/conan.cmake)
set(CONAN_DEPENDENCIES
    mp-units/0.6.0
    linear_algebra/0.7.0@public-conan/testing
    entt/3.6.0
    imgui/1.79
    fmt/7.1.2
    lyra/1.5.1
    nlohmann_json/3.9.1
    tomlplusplus/2.2.0 # need at least version 1.3.3 to get rid of warnings
    tl-optional/1.0.0
    tl-expected/1.0.0
)
if (IMPORT_SDL_FROM STREQUAL "CONAN")
    list(APPEND CONAN_DEPENDENCIES sdl2/2.0.10@bincrafters/stable)
endif()
conan_cmake_configure(BASIC_SETUP REQUIRES ${CONAN_DEPENDENCIES})
conan_cmake_autodetect(settings)
conan_cmake_install(
    PATH_OR_REFERENCE ${CMAKE_BINARY_DIR}
    BUILD missing
    GENERATOR cmake
    SETTINGS ${settings}
)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                           SDL, Threads, GLEW                           #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
message(STATUS "SDL imported from ${IMPORT_SDL_FROM}")
if ("${IMPORT_SDL_FROM}" STREQUAL "CONAN")
    add_library(grv::sdl ALIAS CONAN_PKG::sdl2)
else()
    find_package(SDL2 REQUIRED)
    add_library(grv::sdl ALIAS SDL2::SDL2)
endif()

find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                           imgui integration                            #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
add_library(imgui_sdl_integration OBJECT)
target_sources(imgui_sdl_integration
    PRIVATE
        3rd_party/imgui_sdl_integration/src/imgui_impl_sdl.cpp
        3rd_party/imgui_sdl_integration/src/imgui_impl_opengl3.cpp
)
target_link_libraries(imgui_sdl_integration PUBLIC CONAN_PKG::imgui grv::sdl)
target_include_directories(imgui_sdl_integration PUBLIC 3rd_party/imgui_sdl_integration/include/)


# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                            Enable warnings                             #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
add_library(project_warnings INTERFACE)
include(cmake/compiler_warnings.cmake)
set_project_warnings(project_warnings)


# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
#                                Gravity                                 #
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ #
add_executable(gravity)

target_sources(gravity
    PRIVATE
        src/main.cpp src/common.cpp src/io.cpp src/config.cpp
        src/cli.cpp src/simulation.cpp src/gfx.cpp
        # 3rd_party/src/imgui_impl_opengl3.cpp 3rd_party/src/imgui_impl_sdl.cpp
)
target_compile_features(gravity PUBLIC cxx_std_20)
target_compile_options(gravity PRIVATE)
target_link_libraries(gravity
    PRIVATE
        project_warnings
        grv::sdl OpenGL::GL GLEW::GLEW -lSDL2_gfx imgui_sdl_integration
        CONAN_PKG::mp-units CONAN_PKG::linear_algebra CONAN_PKG::entt CONAN_PKG::imgui
        CONAN_PKG::fmt CONAN_PKG::lyra CONAN_PKG::nlohmann_json CONAN_PKG::tomlplusplus
        CONAN_PKG::tl-optional CONAN_PKG::tl-expected
        Threads::Threads $<$<CXX_COMPILER_ID:GNU>:-ltbb>
)
target_include_directories(gravity
    SYSTEM PRIVATE ./3rd_party/include/ ./SDLpp/include/  # /usr/include/SDL2/
    PRIVATE ./include
)
enable_sanitizers(gravity)
enable_lto(gravity)

