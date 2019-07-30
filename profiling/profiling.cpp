#include <iostream>

#include "Profiling.h"
#include "../utils.h"

#ifdef ss_compile_AMDTAL
#include "CXLActivityLogger.h"
#endif


#ifdef ss_compile_RD
#include "../ss_RenderDoc.h"
#endif

int APIS_INIT;


#define WINEVENT true
#define PIXEVENT true

#if WINEVENT
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Evntprov.h>
#include <Rpc.h>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

std::wstring toWide(const std::string& str) {
	std::wstring str2(str.length(), L' ');
	std::copy(str.begin(), str.end(), str2.begin());
	return str2;
}

REGHANDLE getEventHanld(const std::string& guidStr) {
	REGHANDLE gEventHandle;
	GUID guid;
	ASSERT_BAIL(UuidFromString(((RPC_CSTR)guidStr.data()), &guid) == RPC_S_OK);
	EventRegister(&guid, nullptr, nullptr, &gEventHandle);
	return gEventHandle;
}
#endif


#if PIXEVENT
#include "WinPixEventRuntime/pix3.h"
#endif


struct profiling::marker {
#ifdef ss_compile_AMDTAL
	amdtScopedMarker amdMarker;
#endif
	marker() = delete;
	marker(const std::string& evntTx) : amdMarker(evntTx.data(), NULL) {}
};


void profiling::init(APIFLAGS api)
{
#ifdef ss_compile_RD
	if (api & RENDERDOC) {
		std::cout << "Inject RenderDoc now, if not already attached" << std::endl;
		do {
			std::cout << '\n' << "Press Return to continue...";
		} while (std::cin.get() != '\n');
		RD_init();

		APIS_INIT |= RENDERDOC;
	}
#endif

#ifdef ss_compile_AMDTAL
	if (api & AMDEVENT) {

		std::cout << "Inject AMD Profiler now, if not already attached" << std::endl;
		do {
			std::cout << '\n' << "Press Return to continue...";
		} while (std::cin.get() != '\n');

		auto res = amdtInitializeActivityLogger();
		switch (res)
		{
		case AL_SUCCESS:
			std::cout << '\n' << "AMD Profiler attached";
			break;
		case AL_APP_PROFILER_NOT_DETECTED:
			std::cout << '\n' << "AL_APP_PROFILER_NOT_DETECTED";
			break;
		default:
			BAIL;
			break;
		}
		APIS_INIT |= AMDEVENT;
	}
#endif

	if (api && PIX) {
		APIS_INIT |= PIX;
	}
}

void profiling::deInit()
{
	auto res = amdtFinalizeActivityLogger();
	assert(res == AL_SUCCESS);
}

void profiling::startProfiling(const std::string& evntTxt, APIFLAGS api)
{
#ifdef ss_compile_RD
	if (APIS_INIT && RENDERDOC) {
		RD_StartCapture();
	}
#endif

#ifdef ss_compile_AMDTAL
	if (APIS_INIT && AMDEVENT) {
		amdtResumeProfiling(AMDT_ALL_PROFILING);
	}
#endif
}

void profiling::endProfiling()
{
#ifdef ss_compile_RD
	if (APIS_INIT && RENDERDOC) {
		RD_EndCapture();
	}
#endif

#ifdef ss_compile_AMDTAL
	if (APIS_INIT && AMDEVENT) {
		amdtStopProfiling(AMDT_ALL_PROFILING);
	}
#endif
}

const std::unique_ptr<const profiling::marker> profiling::startMarker(const std::string& evntTxt, APIFLAGS api)
{
	return std::make_unique<profiling::marker>(evntTxt);
}

void profiling::endMarker(std::unique_ptr<const marker> m) {
	m.reset();
}

void profiling::traceEvent(const std::string& evntTxt, APIFLAGS api)
{
	std::cout << "[TrcEvent] " << evntTxt << std::endl;
	{
		auto m = startMarker(evntTxt, ALL);
	}

#if WINEVENT
	static REGHANDLE gEventHandle = getEventHanld("a0000aa0-e5ac-4f2f-be6a-42aad08a9c6f");
	EventWriteString(gEventHandle, 0, 0, toWide(evntTxt).data());
#endif
#if PIXEVENT
	PIXSetMarker(PIX_COLOR(0, 128, 0), evntTxt.data());
#endif

}

#if AMDEVENT
//amdtInitializeActivityLogger()
//amdtBeginMarker()
//amdtEndMarker()
//amdtFinalizeActivityLogger()
//amdtStopProfiling()
//amdtResumeProfiling()
//amdtStopProfilingEx()
//amdtResumeProfilingEx()
#endif