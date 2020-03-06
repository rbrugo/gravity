
cmake_minimum_required(VERSION 3.16.2)

project( gravity
         VERSION 0.1
         DESCRIPTION " ")
# set(CMAKE_CXX_STANDARD 17)
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

if (MSVC)
    target_compile_options( gravity PRIVATE /W4 )
else()
    # target_compile_options( gravity PRIVATE
    #     -Wall -Wextra -Wpedantic -flto $<$<CXX_COMPILER_ID:GNU>:-fconcepts>
    # )
    target_compile_options( gravity PRIVATE
        -Wall -Wextra -Wpedantic $<$<CXX_COMPILER_ID:GNU>:-fconcepts -fsanitize=undefined>
    )
    target_link_options( gravity PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-fsanitize=undefined>
    )

    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined -fsanitize=null -fsanitize=unreachable -fsanitize=return -fsanitize=signed-integer-overflow -fsanitize=bounds -fsanitize=alignment")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined -fsanitize=null -fsanitize=unreachable -fsanitize=return -fsanitize=signed-integer-overflow -fsanitize=bounds -fsanitize=alignment")
endif()

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# add_executable( ${CMAKE_PROJECT_NAME} )
# target_sources( ${CMAKE_PROJECT_NAME} PRIVATE main.cpp )
# target_link_libraries( ${CMAKE_PROJECT_NAME} PUBLIC ${CONAN_LIBS} ${Threads} )
target_sources( gravity PRIVATE src/main.cpp src/common.cpp src/gfx.cpp src/input.cpp )
target_link_libraries( gravity PRIVATE ${CONAN_LIBS} ${Threads} -lSDL2_gfx )

target_include_directories( gravity PRIVATE ./include ./3rd_party/include/)
