# CMakeList.txt : CMake project for Blaze, include source and define
# project specific logic here.
#
cmake_minimum_required( VERSION 3.13 )

find_package( Vulkan REQUIRED )

configure_file("Version.src" "Version.hpp")

set( SOURCE_FILES
        "Blaze2.cpp"
        "Source.cpp"
        "Primitives.cpp"
        "LightSystem.cpp")

set( HEADER_FILES
        "Datatypes.hpp"
        "LightSystem.hpp"
        "Primitives.hpp")

include_directories( "." )
include_directories( "${CMAKE_CURRENT_BINARY_DIR}" )

# Add source to this project's executable.
add_executable( Blaze "Blaze.hpp" "Blaze.cpp" ${SOURCE_FILES} ${HEADER_FILES} )
set_property( TARGET Blaze PROPERTY CXX_STANDARD 17 )

add_subdirectory( "core" )
add_subdirectory( "drawables" )
add_subdirectory( "rendering" )
add_subdirectory( "vkwrap" )
add_subdirectory( "gui" )
add_subdirectory( "util" )
add_subdirectory( "spirv" )
add_subdirectory( "thirdparty" )
add_subdirectory( "shaders" )

# TODO: Add tests and install targets if needed.
# include_directories ("GLFW/include")
# include_directories ("stb")
# include_directories ("glm")

target_link_libraries(Blaze glfw ${GLFW_LIBRARIES})
target_link_libraries(Blaze glm)
target_link_libraries(Blaze Vulkan::Vulkan)

# add_custom_command(TARGET Blaze POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Blaze>/shaders/"
#    COMMAND ${CMAKE_COMMAND} -E copy_directory
#        "${PROJECT_BINARY_DIR}/shaders"
#        "$<TARGET_FILE_DIR:Blaze>/shaders")

file (MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/assets")
file (COPY "${PROJECT_SOURCE_DIR}/Blaze/assets/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/assets")
