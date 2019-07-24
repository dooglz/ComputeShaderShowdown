#include <iostream>
#include <string>
#include "vulkan/bk_vulkan.h"
#include "dx12/bk_dx12.h"
#include "cuda/bk_cuda.h"
#include "opencl/bk_opencl.h"

#define doVk 1
#define doDX12 0
#define doCL 0
#define doCU 0
#define WAIT true
#define useRD true

#if useRD
#include "ss_RenderDoc.h"
#endif

void wait() {
	if (WAIT) {
		do {
			std::cout << '\n' << "Press a key to continue...";
		} while (std::cin.get() != '\n');
	}
}

int main(int argc, const char* const argv[]) {
	(void)argc;
	(void)argv;

	if (useRD) {
		std::cout << "Inject RenderDoc now" << std::endl;
		wait();
		RD_init();
	}

	std::cout << "DEVICE?\n";
	uint8_t dev;
	std::string s = "10";
	std::cin >> s;
	dev = std::stoi(s);

	if (doVk) {
		std::cout << '\n' << std::string(80, '-') << "\nVULKAN\n";
		VK_init(dev);
		if (useRD) { RD_StartCapture(); }
		VK_go(10);
		if (useRD) { RD_EndCapture(); }
		VK_deInit();
		wait();
	}
	if (doDX12) {
		std::cout << '\n' << std::string(80, '-') << "\nDX12\n";
		DX12_init(dev);
		DX12_go(10);
		DX12_deInit();
		wait();
	}
	if (doCL) {
		std::cout << '\n' << std::string(80, '-') << "\nOPENCL\n";
		OPENCL_init(dev);
		OPENCL_go(10);
		OPENCL_deInit();
		wait();
	}
	if (doCU) {
		std::cout << '\n' << std::string(80, '-') << "\nCUDA\n";
		CUDA_init(dev);
		CUDA_go(10);
		CUDA_deInit();
		wait();
	}
	wait();
	return 0;
}