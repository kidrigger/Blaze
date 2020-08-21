
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"HdrTonemap.hpp"
	"Bloom.hpp" )

set( SOURCE_FILES
	"HdrTonemap.cpp"
	"Bloom.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
