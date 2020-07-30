
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"HdrTonemap.hpp" )

set( SOURCE_FILES
	"HdrTonemap.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
