
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"FwdRenderer.hpp"
	"FwdLightCaster.hpp"
	"PointLightCaster.hpp" )

set( SOURCE_FILES
	"FwdRenderer.cpp"
	"FwdLightCaster.cpp"
	"PointLightCaster.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
