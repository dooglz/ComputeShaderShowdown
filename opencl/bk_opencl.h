#include <string>
int OPENCL_init(unsigned char dev = 0);
void OPENCL_go(size_t runs = 10);
int OPENCL_deInit();
int OPENCL_runModule(const std::string& mod, size_t runs);