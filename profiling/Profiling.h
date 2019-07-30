#pragma once
#include <string>
#include <memory>

namespace profiling {
	enum WHICHAPI
	{
		RENDERDOC = 1 << 0,
		AMDEVENT = 1 << 1,
		PIX = 1 << 2,
		ALL = RENDERDOC | AMDEVENT | PIX
	};
	typedef int APIFLAGS;
	struct marker;

	void init(APIFLAGS api = ALL);
	void deInit();

	void startProfiling(const std::string& evntTxt, APIFLAGS api = ALL);
	void endProfiling();

	//Markers should always be within a profiling range
	const std::unique_ptr<const marker> startMarker(const std::string& evntTxt, APIFLAGS api = ALL);
	void endMarker(std::unique_ptr<const marker>);

	//Trace events are 0 width markers
	void traceEvent(const std::string& evntTxt, APIFLAGS api = ALL);
}