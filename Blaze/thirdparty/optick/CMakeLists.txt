
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"optick.config.h"
	"optick.h"
	"optick_capi.h"
	"optick_common.h"
	"optick_core.freebsd.h"
	"optick_core.h"
	"optick_core.linux.h"
	"optick_core.macos.h"
	"optick_core.platform.h"
	"optick_core.win.h"
	"optick_gpu.h"
	"optick_memory.h"
	"optick_message.h"
	"optick_miniz.h"
	"optick_serialization.h"
	"optick_server.h" )

set( SOURCE_FILES
	"optick_capi.cpp"
	"optick_core.cpp"
	"optick_gpu.cpp"
	"optick_gpu.d3d12.cpp"
	"optick_gpu.vulkan.cpp"
	"optick_message.cpp"
	"optick_miniz.cpp"
	"optick_serialization.cpp"
	"optick_server.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )