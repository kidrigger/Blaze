# CMakeList.txt : CMake project for Blaze, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

find_package(Vulkan REQUIRED)

# Add source to this project's executable.
add_executable (Blaze "Blaze.cpp" "Blaze.hpp")

target_compile_definitions(Blaze PRIVATE VK_USE_PLATFORM_WIN32_KHR)

# TODO: Add tests and install targets if needed.
include_directories ("glfw/include")
include_directories ("glm")

target_link_libraries(Blaze glfw ${GLFW_LIBRARIES})
target_link_libraries(Blaze glm)
target_link_libraries(Blaze Vulkan::Vulkan)