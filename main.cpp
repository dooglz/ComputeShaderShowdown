#include <iostream>
#include <string>

#include "profiling/Profiling.h"



#ifdef ss_compile_VK
#include "vulkan/bk_vulkan.h"
#endif

#ifdef ss_compile_CL
#include "opencl/bk_opencl.h"
#endif 

#ifdef ss_compile_CU
#include "cuda/bk_cuda.h"
#endif

#ifdef ss_compile_DX
#include "dx12/bk_dx12.h"
#endif 

#include "deps/CLI11/include/CLI/CLI.hpp"


bool WAIT = false;
void wait() {
	if (WAIT) {
		do {
			std::cout << '\n' << "Press Retrun to continue...";
		} while (std::cin.get() != '\n');
	}
}

int main(int argc, char** argv) {
	CLI::App app{ "Shader Showdown" };

	bool doVk = false;
	bool doDX12 = false;
	bool doCL = false;
	bool doCU = false;
	bool useRD = false;
	bool useAMDTAL = false;
	bool all = false;

#ifdef ss_compile_RD
	app.add_flag("--rd,--renderdoc", useRD, "load & use RenderDoc API");
#endif

#ifdef ss_compile_AMDTAL
	app.add_flag("--codeXL,--amdtal,--AMDTActivityLogger", useAMDTAL, "load & use AMDTAL API");
#endif

	app.add_flag("-w,--wait", WAIT, "prompt for user input between tests");

#ifdef ss_compile_VK
	app.add_flag("--vk,--vulkan", doVk, "Vulkan");
#endif

#ifdef ss_compile_CL
	app.add_flag("--cl,--opencl", doCL, "CL");
#endif 

#ifdef ss_compile_CU
	app.add_flag("--cu,--cuda", doCU, "CU");
#endif 

#ifdef ss_compile_DX
	app.add_flag("--dx,--dx12", doDX12, "DX12");
#endif 

	app.add_flag("-a,--all", all, "run all ");
	uint8_t dev = 0;
	app.add_option("-d,--device", dev, "Device to run on");

	CLI11_PARSE(app, argc, argv);

	if (all) {
		doVk = true;
		doDX12 = true;
		doCL = true;
		doCU = true;
	}

	profiling::init((useAMDTAL * profiling::AMDEVENT)| (useRD * profiling::RENDERDOC));


#ifdef ss_compile_VK
	if (doVk) {
		std::cout << '\n' << std::string(80, '-') << "\nVULKAN\n";
		profiling::startProfiling("Init");
		VK_init(dev);
		profiling::endProfiling();
		profiling::startProfiling("Go");
		VK_go(10);
		profiling::endProfiling();
		VK_deInit();
		wait();
	}
#endif 
#ifdef ss_compile_DX
	if (doDX12) {
		std::cout << '\n' << std::string(80, '-') << "\nDX12\n";
		DX12_init(dev);
		DX12_go(10);
		DX12_deInit();
		wait();
	}
#endif 
#ifdef ss_compile_CL
	if (doCL) {
		std::cout << '\n' << std::string(80, '-') << "\nOPENCL\n";
		OPENCL_init(dev);
		OPENCL_go(10);
		OPENCL_deInit();
		wait();
	}
#endif 
#ifdef ss_compile_CU
	if (doCU) {
		std::cout << '\n' << std::string(80, '-') << "\nCUDA\n";
		CUDA_init(dev);
		CUDA_go(10);
		CUDA_deInit();
		wait();
	}
#endif 
	wait();
	return 0;
}