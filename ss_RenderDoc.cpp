#include "ss_RenderDoc.h"
//Look, I know I'm goin to be the only person who runs this code, shoot me
#include "G:/renderdoc-1.x/renderdoc/api/app/renderdoc_app.h"
#include "utils.h"
#define _AMD64_
#include <Libloaderapi.h>
#include <iostream>


RENDERDOC_API_1_4_0* rdoc_api = NULL;

void RD_init()
{


#ifdef _MSC_VER

	if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
	{
		pRENDERDOC_GetAPI RENDERDOC_GetAPI =
			(pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
		int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_0, (void**)& rdoc_api);
		assert(ret == 1);
		int m1, m2, m3;
		rdoc_api->GetAPIVersion(&m1, &m2, &m3);
		std::cout << "Rd module loaded Version:" << m1 << '.' << m2 << '.' << m2 << std::endl;
	}
	else {
		std::cerr << "no Rd module loaded" << std::endl;
	}
#endif // _MSC_VER

}

void RD_StartCapture()
{
	if (rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);

}
void RD_EndCapture()
{
	if (rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);
}