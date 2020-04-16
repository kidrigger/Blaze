# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required( VERSION 3.13 )

project( Blaze  VERSION 0.0.1 )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

# Include sub-projects.
add_subdirectory( "Blaze" )
add_subdirectory( "glfw" )

set( GLM_TEST_ENABLE OFF )
add_subdirectory( "glm" )
