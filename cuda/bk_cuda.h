#pragma once
#include <string>
int CUDA_init(unsigned char dev = 0);
void CUDA_go(size_t runs = 10);
int CUDA_deInit();

int CUDA_runModule(const std::string& mod, size_t runs);