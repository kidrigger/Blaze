
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"spirv.h"
	"spirv_reflect.h" )

set( SOURCE_FILES
	"spirv_reflect.c" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
