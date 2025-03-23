cmake_minimum_required(VERSION 3.31.0)

project(jubilant VERSION 0.0.1 LANGUAGES C)

# CMake options
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG) 
endif()


# Fetch dependencies
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
  cglm
  GIT_REPOSITORY https://github.com/recp/cglm.git
  GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(cglm)

# find dependancies
# find_package(Vulkan REQUIRED)

# Define targets
add_subdirectory(src)
