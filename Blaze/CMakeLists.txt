# CMakeList.txt : CMake project for Blaze, include source and define
# project specific logic here.
#
cmake_minimum_required( VERSION 3.13 )

find_package( Vulkan REQUIRED )

set( SOURCE_FILES
        "TextureCube.cpp"
        "Texture2D.cpp"
        "Swapchain.cpp"
        "Primitives.cpp"
        "Model.cpp"
        "Renderer.cpp"
        "LightSystem.cpp"
        "GUI.cpp" )

set( HEADER_FILES
        "Camera.hpp"
        "Datatypes.hpp"
        "Drawable.hpp"
        "GUI.hpp"
        "LightSystem.hpp"
        "Model.hpp"
        "Primitives.hpp"
        "Renderer.hpp"
        "Swapchain.hpp"
        "Texture2D.hpp"
        "TextureCube.hpp"
        "UniformBuffer.hpp"
        "VertexBuffer.hpp" )

include_directories( "vma" )
include_directories( "gltf" )
include_directories( "." )

include_directories( "../spirv-cross" )

# Add source to this project's executable.
add_executable( Blaze "Blaze.hpp" "Blaze.cpp" ${SOURCE_FILES} ${HEADER_FILES} )

add_subdirectory( "core" )
add_subdirectory( "gltf" )
add_subdirectory( "gui" )
add_subdirectory( "util" )
add_subdirectory( "vma" )

if( WIN32 )
    target_compile_definitions( Blaze PRIVATE VK_USE_PLATFORM_WIN32_KHR )
    if( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64" )
      set( GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe" )
    else()
      set( GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe" )
    endif()
elseif( UNIX AND NOT APPLE )
    target_compile_definitions( Blaze PRIVATE VK_USE_PLATFORM_X11_KHR )
    find_program( GLSL_VALIDATOR NAMES "glslangValidator" )
endif()

# TODO: Add tests and install targets if needed.
# include_directories ("GLFW/include")
# include_directories ("stb")
# include_directories ("glm")

target_link_libraries(Blaze glfw ${GLFW_LIBRARIES})
target_link_libraries(Blaze glm)
target_link_libraries(Blaze Vulkan::Vulkan)
target_link_libraries(Blaze spirv-cross-glsl
			spirv-cross-hlsl
			spirv-cross-cpp
			spirv-cross-reflect
			spirv-cross-msl
			spirv-cross-util
			spirv-cross-core)

# Credits vlsh https://gist.github.com/vlsh/a0d191701cb48f157b05be7f74d79396
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "shaders/*.frag"
    "shaders/*.vert"
	"shaders/*.vs"
	"shaders/*.fs"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )

add_dependencies(Blaze Shaders)

add_custom_command(TARGET Blaze PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Blaze>/shaders/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_BINARY_DIR}/shaders"
        "$<TARGET_FILE_DIR:Blaze>/shaders"
)

file (MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/assets")
file (COPY "${PROJECT_SOURCE_DIR}/Blaze/assets/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/assets")
