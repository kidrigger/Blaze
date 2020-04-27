
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"VkWrap.hpp"
	"VkWrapSpecialized.hpp"
)

target_sources( Blaze PRIVATE ${HEADER_FILES} )
