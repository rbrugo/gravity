######################################################################
# @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
# @file        : CMakeLists
# @created     : Sunday Mar 21, 2021 16:16:27 CET
######################################################################

add_library(imgui_sdl_integration OBJECT)
target_sources(imgui_sdl_integration PRIVATE src/imgui_impl_sdl.cpp src/imgui_impl_opengl3.cpp)
target_include_directories(imgui_sdl_integration PUBLIC include/)
