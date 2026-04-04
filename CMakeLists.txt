cmake_minimum_required(VERSION 3.20)
project(Orbitals LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)

FetchContent_Declare(
  glew
  URL https://sourceforge.net/projects/glew/files/glew/2.2.0/glew-2.2.0.tgz/download
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)

FetchContent_MakeAvailable(glfw glm)

FetchContent_GetProperties(glew)
if(NOT glew_POPULATED)
  FetchContent_Populate(glew)
endif()

add_library(glew STATIC
  ${glew_SOURCE_DIR}/src/glew.c
)

target_include_directories(glew PUBLIC
  ${glew_SOURCE_DIR}/include
)

target_compile_definitions(glew PUBLIC
  GLEW_STATIC
)

if(MSVC)
  set_target_properties(glew PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
  )
endif()

find_package(OpenGL REQUIRED)

add_executable(orbitals
  src/main.cpp
)

if(MSVC)
  set_target_properties(orbitals PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
  )
endif()

target_include_directories(orbitals PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(orbitals PRIVATE
  glfw
  glew
  glm::glm
  OpenGL::GL
)

target_compile_definitions(orbitals PRIVATE
  GLEW_STATIC
)

if (MSVC)
  target_compile_options(orbitals PRIVATE /W4 /permissive-)
else()
  target_compile_options(orbitals PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET orbitals POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:orbitals>/shaders"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/orbitals.vert"
    "$<TARGET_FILE_DIR:orbitals>/shaders/orbitals.vert"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/orbitals_template.frag"
    "$<TARGET_FILE_DIR:orbitals>/shaders/orbitals_template.frag"
  COMMENT "Copying shader files next to executable"
)
