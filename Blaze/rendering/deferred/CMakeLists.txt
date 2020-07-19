
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"DfrRenderer.hpp"
	"DfrLightCaster.hpp"
	"PointLightCaster.hpp"
	"DirectionLightCaster.hpp" )

set( SOURCE_FILES
	"DfrRenderer.cpp" 
	"DfrLightCaster.cpp"
	"PointLightCaster.cpp"
	"DirectionLightCaster.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
