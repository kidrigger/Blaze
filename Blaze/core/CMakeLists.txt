
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"Context.hpp"
	"Swapchain.hpp"
	"Camera.hpp"
	"UniformBuffer.hpp"
	"VertexBuffer.hpp"
    "Texture2D.hpp"
    "TextureCube.hpp"
	"Drawable.hpp" )

set( SOURCE_FILES
	"Context.cpp"
	"Swapchain.cpp"
	"UniformBuffer.cpp"
	"VertexBuffer.cpp"
    "TextureCube.cpp"
    "Texture2D.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
