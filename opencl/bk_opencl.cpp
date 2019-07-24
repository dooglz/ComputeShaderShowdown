#include "bk_opencl.h"
#include "../utils.h"
#include <CL/cl.hpp>
#include <iostream>

inline void checkErr(cl_int err, const char* name = "error")
{
	if (err != CL_SUCCESS) {
		std::cerr << "ERROR: " << name << " (" << err << ")" << std::endl;
		BAIL
	}
}
std::string toString(int dt) {
	std::string s("");
	if (dt & CL_DEVICE_TYPE_CPU)
		s += "TYPE_CPU ";
	if (dt & CL_DEVICE_TYPE_GPU)
		s += "TYPE_GPU ";
	if (dt & CL_DEVICE_TYPE_ACCELERATOR)
		s += "TYPE_ACCELERATOR ";
	if (dt & CL_DEVICE_TYPE_DEFAULT)
		s += "TYPE_DEFAULT ";

	return s;
}

int OPENCL_init(unsigned char dev) {
	traceEvent("CL Init");
	cl_int err;
	std::vector< cl::Platform > platformList;
	cl::Platform::get(&platformList);
	BAIL_IF(platformList.size(), 0);

	std::cout << platformList.size() << " OpenCL Platforms\n";

	for (auto& plt : platformList) {
		std::string vendor, name;
		plt.getInfo((cl_platform_info)CL_PLATFORM_VENDOR, &vendor);
		plt.getInfo(CL_PLATFORM_NAME, &name);
		std::cout << "OpenCL Platform: " << name << " by: " << vendor << "\n";

		std::vector<cl::Device> devices;
		plt.getDevices(CL_DEVICE_TYPE_ALL, &devices);
		std::cout << devices.size() << " Platform devices\n";
		for (auto& dv : devices) {
			std::string vendor, name;
			cl_uint cus;
			int type;
			dv.getInfo(CL_DEVICE_VENDOR, &vendor);
			dv.getInfo(CL_DEVICE_NAME, &name);
			dv.getInfo(CL_DEVICE_TYPE, &type);
			dv.getInfo(CL_DEVICE_MAX_COMPUTE_UNITS, &cus);
			std::cout << "\tDevice: " << name << " - " << vendor << " - " << toString(type) << " - " << cus << "\n";
		}
		std::cout << std::endl;
	}
	/*


	cl::Context context(
		CL_DEVICE_TYPE_CPU,
		cprops,
		NULL,
		NULL,
		&err);
	checkErr(err, "Conext::Context()");

	*/

	return 0;

}


void OPENCL_go(size_t runs) {}
int OPENCL_deInit() { return 0; }
