#pragma once
#include <string>
int DX12_init(unsigned char dev = 0);
void DX12_go(size_t runs = 10);
int DX12_deInit();
int DX12_runModule(const std::string& mod, size_t runs);