cmake_minimum_required(VERSION 3.16)
project(mujoco_opengl_demo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch MuJoCo source directly (and its internal dependencies like GLFW)
include(FetchContent)
FetchContent_Declare(
  mujoco
  GIT_REPOSITORY https://github.com/google-deepmind/mujoco.git
  GIT_TAG        main # Or point to a specific release tag like 3.2.0
)
FetchContent_MakeAvailable(mujoco)

# Define our executable target
add_executable(sim_demo main.cpp)

# Target dependencies (ADDED 'glfw' here so main.cpp can find <GLFW/glfw3.h>)
target_link_libraries(sim_demo PRIVATE mujoco::mujoco glfw)

# For macOS, explicitly link necessary graphics frameworks
if(APPLE)
    target_link_libraries(sim_demo PRIVATE "-framework Cocoa" "-framework OpenGL" "-framework IOKit")
endif()
