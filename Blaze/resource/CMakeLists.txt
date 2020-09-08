
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"Model.hpp"
	"Node.hpp"
	"Environment.hpp"
	"ModelLoader.hpp" )

set( SOURCE_FILES
	"Node.cpp"
	"Model.cpp"
	"Environment.cpp"
	"ModelLoader.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
