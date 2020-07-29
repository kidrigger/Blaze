
#include "renderdoc_api.h"
#include "renderdoc.h"
#include <windows.h>

namespace renderdoc
{
	static RENDERDOC_API_1_1_2* rdoc_api = NULL;

	void init()
	{
		// At init, on windows
		if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
		{
			pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
			int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
			assert(ret == 1);
		}
	}

	void startCapture()
	{
		// To start a frame capture, call StartFrameCapture.
		// You can specify NULL, NULL for the device to capture on if you have only one device and
		// either no windows at all or only one window, and it will capture from that device.
		// See the documentation below for a longer explanation
		if (rdoc_api)
			rdoc_api->StartFrameCapture(NULL, NULL);
	}

	void endCapture()
	{
		// stop the capture
		if (rdoc_api)
			rdoc_api->EndFrameCapture(NULL, NULL);
	}
}
