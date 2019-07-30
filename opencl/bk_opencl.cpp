#include "bk_opencl.h"
#include "../utils.h"
#include "../profiling/Profiling.h"
#define CL_HPP_ENABLE_EXCEPTIONS
//Because  Nvidia is lame
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_TARGET_OPENCL_VERSION 120
#include "../deps/AMD-Opencl/3-0/include/CL/cl2.hpp"
#include <iostream>
#include <vector>

const int32_t bufferLength = 16384;
const uint32_t bufferSize = sizeof(int32_t) * bufferLength;

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

cl::Platform platform;
cl::Device device;
std::vector<int32_t> payload;
cl::Buffer inputBuffer;
cl::Buffer outputBuffer;

std::unique_ptr<cl::KernelFunctor<cl::Buffer, cl::Buffer >> kernelProgramFunc;

int OPENCL_init(unsigned char dev) {
	profiling::traceEvent("CL Init");
	//cl_int err;

	std::vector< cl::Platform > platformList;
	cl::Platform::get(&platformList);
	BAIL_IF(platformList.size(), 0);
	assert(dev<platformList.size());

	std::cout << platformList.size() << " OpenCL Platforms\n";

	for (auto& plt : platformList) {
		std::cout << "OpenCL Platform: " << plt.getInfo<CL_PLATFORM_NAME>() << " by: " << plt.getInfo<CL_PLATFORM_VENDOR>() << " Version:" << plt.getInfo<CL_PLATFORM_VERSION>() << "\n";
		std::vector<cl::Device> devices;
		plt.getDevices(CL_DEVICE_TYPE_ALL, &devices);
		std::cout << devices.size() << " Platform devices\n";
		for (auto& dv : devices) {
			std::cout << "\tDevice: " << dv.getInfo<CL_DEVICE_NAME>() <<
				" - " << dv.getInfo<CL_DEVICE_VENDOR>() << " - "
				<< toString(dv.getInfo<CL_DEVICE_TYPE>()) << " - "
				<< dv.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << "\n";
		}
		std::cout << std::endl;
	}

	platform = cl::Platform::setDefault(platformList[dev]);
	{
		std::vector<cl::Device> devices;
		platform.getDevices(CL_DEVICE_TYPE_ALL,&devices);
		BAIL_IF(devices.size(), 0);
		device = cl::Device::setDefault(devices[0]);
	}
	//device = cl::Device::getDefault();
	std::cout << "using " << device.getInfo<CL_DEVICE_NAME>() << std::endl;

	std::string kernel{ R"(
		kernel void CSMain(global const int *input, global int *output)
		{
		  output[get_global_id(0)] = input[get_global_id(0)] * 2;
		}
	)" };

	profiling::traceEvent("CL Compile Kernel");
	cl::Program kernelProgram(kernel);
	try {
		kernelProgram.build("-cl-std=CL1.2");
	}
	catch (...) {
		// Print build info for all devices
		cl_int buildErr = CL_SUCCESS;
		auto buildInfo = kernelProgram.getBuildInfo<CL_PROGRAM_BUILD_LOG>(&buildErr);
		for (auto& pair : buildInfo) {
			std::cerr << pair.second << std::endl << std::endl;
		}
		BAIL
	}

	profiling::traceEvent("CL Compiled");

	{
		//Could extract binary here if you wanted
		auto binaries = kernelProgram.getInfo<CL_PROGRAM_BINARIES>();
		assert(binaries.size() > 0);
		std::cout << "Binary Size:" << binaries[0].size() << std::endl;
	}

	payload = std::vector<int32_t>(bufferLength, 0xdeadbeef);
	for (uint32_t k = 1; k < bufferLength; k++) {
		payload[k] = (k % 10);//rand();
	}

	inputBuffer = cl::Buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bufferSize, payload.data());
	outputBuffer = cl::Buffer(CL_MEM_WRITE_ONLY, bufferSize);
	kernelProgramFunc = std::make_unique<cl::KernelFunctor<cl::Buffer, cl::Buffer >>(kernelProgram, "CSMain");
	return 0;

}


void OPENCL_go(size_t runs) {

	profiling::traceEvent("CL Execute");
	const cl::EnqueueArgs launchArgs(bufferLength);
	for (size_t i = 0; i < runs; i++)
	{
		std::cout << "Submit\n";
		auto t1 = startTimer();
		(*kernelProgramFunc)(launchArgs, inputBuffer, outputBuffer);
		std::cout << "Done " << endtimer(t1) << "ns \n";
	}

	profiling::traceEvent("CL End");
	cl::copy(outputBuffer, begin(payload), end(payload));
	std::cout << printSome(payload.data(), bufferLength);

}
int OPENCL_deInit() {
	return 0;
}
