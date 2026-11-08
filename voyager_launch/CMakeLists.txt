cmake_minimum_required(VERSION 3.10)
project(voyager_launch CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Needs: sudo apt install build-essential cmake libglfw3-dev libglew-dev libglm-dev libgl1-mesa-dev
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_path(GLM_INCLUDE_DIR glm/glm.hpp)

set(ASSET_DIR "${CMAKE_SOURCE_DIR}/assets/")

function(add_app app_name source_file)
  add_executable(${app_name} src/apps/${source_file})
  target_include_directories(${app_name} PRIVATE src/common third_party ${GLM_INCLUDE_DIR})
  target_compile_definitions(${app_name} PRIVATE ASSET_DIR="${ASSET_DIR}")
  target_link_libraries(${app_name} PRIVATE glfw ${GLEW_LIBRARIES} OpenGL::GL)
endfunction()

add_app(a2-model-0 a2_model_0.cpp)
add_app(a2-model-1 a2_model_1.cpp)
add_app(a2-model-2 a2_model_2.cpp)
add_app(a2-scene   a2_scene.cpp)
# Assignment 3: interactive trajectory editor / keyframe playback
add_executable(a3-trajectory
  src/apps/a3_trajectory.cpp
  src/trajectory/bezier.cpp
  src/trajectory/trajectory_io.cpp
  src/trajectory/keyframes.cpp
  src/trajectory/mission_paths.cpp
)
target_include_directories(a3-trajectory PRIVATE
  src/common src/trajectory third_party ${GLM_INCLUDE_DIR}
)
target_compile_definitions(a3-trajectory PRIVATE ASSET_DIR="${ASSET_DIR}")
target_link_libraries(a3-trajectory PRIVATE glfw ${GLEW_LIBRARIES} OpenGL::GL)
