#pragma once
#include "utils.h"
#include <sstream>
#include <numeric>
#include <vector>
#include <algorithm>
#include <sstream>

#include <iostream>
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


void traceEvent(const std::string& evntTxt)
{

	std::cout << "[TrcEvent] " << evntTxt << std::endl;
#if WINEVENT
	static REGHANDLE gEventHandle = getEventHanld("a0000aa0-e5ac-4f2f-be6a-42aad08a9c6f");
	EventWriteString(gEventHandle, 0, 0, toWide(evntTxt).data());
#endif
#if PIXEVENT
	PIXSetMarker(PIX_COLOR(0, 128, 0) , evntTxt.data());
#endif
}


std::chrono::time_point<chronoclock> startTimer() {
	return chronoclock::now();
}
long long endtimer(std::chrono::time_point<chronoclock> tp) {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(chronoclock::now() - tp).count();
}

std::string printSome(int32_t* data, size_t length) {
	std::vector<int> a;
	a.resize(std::min(length, (size_t)10));
	std::iota(a.begin(), a.end() - 2, 0);
	*(a.end() - 2) = length / (size_t)2;
	*(a.end() - 1) = length - (size_t)1;
	std::stringstream ss;
	for (const int& i : a) {
		ss << "[" << std::to_string(i) << "] " << std::to_string(data[i]) << ", ";
	}
	return ss.str().substr(0, ss.str().length() - 2);
}

