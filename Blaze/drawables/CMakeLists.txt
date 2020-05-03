
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"Model.hpp"
	"Model2.hpp"
	"Node.hpp"
	"Material.hpp"
	"ModelLoader.hpp" )

set( SOURCE_FILES
	"Model.cpp" 
	"Node.cpp"
	"Material.cpp"
	"loadModel.cpp"
	"Model2.cpp"
	"ModelLoader.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
