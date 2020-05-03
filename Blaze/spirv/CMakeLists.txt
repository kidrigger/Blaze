
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES "PipelineFactory.hpp" "Pipeline.hpp" )

set( SOURCE_FILES "PipelineFactory.cpp" "Pipeline.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
