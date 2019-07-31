#pragma once
#include <string>


#define execIf(x,...) auto __tf=x;if(__tf){__tf(__VA_ARGS__);}
#define execFunc(m,x,...) execIf(m.func<x>(#x),__VA_ARGS__);

class ExternalModule {
	ExternalModule() = delete;
private:
	const void* _module;
	void* _getProcAddress(const std::string& functionName);
public:
	template<typename Tf>
	Tf func(const std::string& functionName)
	{
		const auto f = (Tf)_getProcAddress(functionName);
		if (!f) {
			std::cout << "Module has no function "<< functionName << std::endl;
		}
		return f;
	}
	ExternalModule(const std::string& modulePath);
	~ExternalModule();
};

