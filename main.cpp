#include <iostream>

#include "vulkan/bk_vulkan.h"
#include "dx12/bk_dx12.h"
#include "cuda/bk_cuda.h"
#include "opencl/bk_opencl.h"

#define doVk 1
#define doDX12 1
#define doCL 1
#define doCU 1

int main(int argc, const char* const argv[]) {
	(void)argc;
	(void)argv;
	if (doVk) {
		std::cout << '\n' << std::string(80, '-') << "\nVULKAN\n";
		VK_init();
		VK_go(10);
		VK_deInit();
	}
	if (doDX12) {
		std::cout << '\n' << std::string(80, '-') << "\nDX12\n";
		DX12_init();
		DX12_go(10);
		DX12_deInit();
	}
	if (doCL) {
		std::cout << '\n' << std::string(80, '-') << "\nOPENCL\n";
		OPENCL_init();
		OPENCL_go(10);
		OPENCL_deInit();
	}
	if (doCU) {
		std::cout << '\n' << std::string(80, '-') << "\nCUDA\n";
		CUDA_init();
		CUDA_go(10);
		CUDA_deInit();
	}
	return 0;
}