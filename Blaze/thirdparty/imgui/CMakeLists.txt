
cmake_minimum_required( VERSION 3.13 )

set( HEADER_FILES
	"imconfig.h"
	"imgui.h"
	"imgui_impl_glfw.h"
	"imgui_impl_vulkan.h"
	"imgui_internal.h"
	"imstb_rectpack.h"
	"imstb_textedit.h"
	"imstb_truetype.h" )

set( SOURCE_FILES
	"imgui.cpp"
	"imgui_demo.cpp"
	"imgui_draw.cpp"
	"imgui_impl_glfw.cpp"
	"imgui_impl_vulkan.cpp"
	"imgui_widgets.cpp" )

target_sources( Blaze PRIVATE ${HEADER_FILES} ${SOURCE_FILES} )
