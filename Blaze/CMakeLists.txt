# CMakeList.txt : CMake project for Blaze, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

find_package(Vulkan REQUIRED)

file(GLOB_RECURSE SOURCE_FILES
	"*.cpp"
	"util/*.cpp"
	"vma/*.cpp"
	"gltf/*.cpp"
	)

file(GLOB_RECURSE HEADER_FILES
	"*.hpp"
	"util/*.hpp"
	"vma/*.h"
	"gltf/*.h"
	"gltf/*.hpp"
	)

include_directories ("vma")
include_directories ("gltf")
include_directories (".")

# Add source to this project's executable.
add_executable (Blaze ${SOURCE_FILES} ${HEADER_FILES})

target_compile_definitions(Blaze PRIVATE VK_USE_PLATFORM_WIN32_KHR)

# TODO: Add tests and install targets if needed.
# include_directories ("glfw/include")
# include_directories ("stb")
# include_directories ("glm")

target_link_libraries(Blaze glfw ${GLFW_LIBRARIES})
target_link_libraries(Blaze glm)
target_link_libraries(Blaze Vulkan::Vulkan)

# Credits vlsh https://gist.github.com/vlsh/a0d191701cb48f157b05be7f74d79396
if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else()
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()

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

#file (GLOB TEXTURES "assets/container2.png")
#add_custom_command(TARGET Blaze POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Blaze>/assets/"
#	COMMAND ${CMAKE_COMMAND} -E copy "${TEXTURES}" "$<TARGET_FILE_DIR:Blaze>/assets/"
#)

#file (GLOB_RECURSE SPONZA "assets/sponza/*")
#add_custom_command(TARGET Blaze POST_BUILD
#	COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Blaze>/assets/sponza"
#	COMMAND ${CMAKE_COMMAND} -E copy "${SPONZA}" "$<TARGET_FILE_DIR:Blaze>/assets/sponza"
#)

#file (GLOB_RECURSE SPONZA "assets/helmet/*")
#add_custom_command(TARGET Blaze POST_BUILD
#	COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Blaze>/assets/helmet"
#	COMMAND ${CMAKE_COMMAND} -E copy "${SPONZA}" "$<TARGET_FILE_DIR:Blaze>/assets/helmet"
#)

file (MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/assets")
file (COPY "${PROJECT_SOURCE_DIR}/Blaze/assets/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/assets")