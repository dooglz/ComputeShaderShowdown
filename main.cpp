#include "vulkan/bk_vulkan.h"
#include "dx12/bk_dx12.h"
#define doVk false
#define doDX12 true

int main(int argc, const char* const argv[]) {
	(void)argc;
	(void)argv;
	if (doVk) {
		VK_init();
		VK_go(10);
		VK_deInit();
	}
	if (doDX12) {
		DX12_init();
		DX12_go(10);
		DX12_deInit();
	}
}