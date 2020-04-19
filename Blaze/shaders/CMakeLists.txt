
cmake_minimum_required(VERSION 3.13)

# Credits vlsh https://gist.github.com/vlsh/a0d191701cb48f157b05be7f74d79396
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "*.frag"
    "*.vert"
	"*.vs"
	"*.fs"
    )

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

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  file(RELATIVE_PATH FILE_REL_PATH ${PROJECT_SOURCE_DIR} ${GLSL})
  set(SPIRV "${PROJECT_BINARY_DIR}/${FILE_REL_PATH}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    # COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    SPIRV_Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )

add_dependencies(Blaze SPIRV_Shaders)