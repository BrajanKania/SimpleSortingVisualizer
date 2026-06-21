cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  else()
    message(WARNING "Not found VCPKG_ROOT. Use -DCMAKE_TOOLCHAIN_FILE=YOUR_PATH/scripts/buildsystems/vcpkg.cmake.")
  endif()
endif()

project(SortingVisualizer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(SortingVisualizer)

target_sources(SortingVisualizer PRIVATE main.cpp)

include(FetchContent)

FetchContent_Declare(
    glad2
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        e3c14f906127dea49ead20ff4d5878294d08b7ab
    SOURCE_SUBDIR  cmake
)
FetchContent_MakeAvailable(glad2)
glad_add_library(glad2_lib STATIC API gl:core=4.6)

find_package(OpenGL REQUIRED COMPONENTS OpenGL)
find_package(SDL3 REQUIRED)
find_package(glm REQUIRED)

target_link_libraries(SortingVisualizer
  PRIVATE
    OpenGL::OpenGL
    SDL3::SDL3
    glm::glm
    glad2_lib
)

