
cmake_minimum_required(VERSION 3.16.2)

project( gravity
         VERSION 0.1
         DESCRIPTION " ")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable( gravity )
target_compile_features( gravity PUBLIC cxx_std_20 )
find_package(Threads REQUIRED)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_program(CCACHE_FOUND "ccache")
set(CCACHE_SUPPORT ON CACHE BOOL "Enable ccache support")
if (CCACHE_FOUND AND CCACHE_SUPPORT)
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" # GNU is GNU GCC
      OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # without this compiler messages in `make` backend would be uncolored
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fdiagnostics-color=auto")
  endif()
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "ccache")
endif()

target_compile_definitions(gravity PUBLIC GRAVITY_NO_JSON) # nlohmann_json/3.7.3 is incompatible with GCC10
target_compile_options(gravity PRIVATE -Wall -Wextra -Wpedantic -fconcepts)
target_link_options(gravity PRIVATE)

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined -fsanitize=null -fsanitize=unreachable -fsanitize=return -fsanitize=signed-integer-overflow -fsanitize=bounds -fsanitize=alignment")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined -fsanitize=null -fsanitize=unreachable -fsanitize=return -fsanitize=signed-integer-overflow -fsanitize=bounds -fsanitize=alignment")

target_sources(gravity PRIVATE src/main.cpp src/common.cpp src/io.cpp src/config.cpp src/cli.cpp src/simulation.cpp src/gfx.cpp 3rd_party/src/imgui_impl_opengl3.cpp 3rd_party/src/imgui_impl_sdl.cpp)
target_link_libraries(gravity PRIVATE -lGL -lGLEW ${CONAN_LIBS} ${Threads} -lSDL2_gfx -lsndio -ltbb)

target_include_directories(gravity PRIVATE ./include SYSTEM ./3rd_party/include/)
