
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"Renderer.hpp"
	"ForwardRenderer.hpp"
	"ARenderer.hpp"
	"FwdRenderer.hpp"
	"ALightCaster.hpp"
	"FwdLightCaster.hpp" )

set( SOURCE_FILES
	"ForwardRenderer.cpp"
	"ARenderer.cpp"
	"FwdRenderer.cpp"
	"FwdLightCaster.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
