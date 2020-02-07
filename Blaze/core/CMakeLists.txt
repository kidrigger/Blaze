
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"Pipeline.hpp"
	"Context.hpp" )

set( SOURCE_FILES
	"Context.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
