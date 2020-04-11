
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"createFunctions.hpp"
	"debugMessenger.hpp"
	"DebugTimer.hpp"
	"DeviceSelection.hpp"
	"files.hpp"
	"Managed.hpp"
	"processing.hpp"
	"ShaderUtils.hpp"
	"Environment.hpp")

set( SOURCE_FILES
	"createFunctions.cpp"
	"debugMessenger.cpp"
	"DeviceSelection.cpp"
	"files.cpp"
	"processing.cpp"
	"Environment.cpp")

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
