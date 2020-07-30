
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"ARenderer.hpp"
	"ALightCaster.hpp" )

set( SOURCE_FILES
	"ARenderer.cpp")

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )

add_subdirectory( "forward" )
add_subdirectory( "deferred" )
add_subdirectory( "postprocess" )