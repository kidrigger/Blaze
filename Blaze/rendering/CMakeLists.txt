
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"Renderer.hpp"
	"ForwardRenderer.hpp"
	"ARenderer.hpp"
	"FwdRenderer.hpp" )

set( SOURCE_FILES
	"ForwardRenderer.cpp"
	"ARenderer.cpp"
	"FwdRenderer.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
