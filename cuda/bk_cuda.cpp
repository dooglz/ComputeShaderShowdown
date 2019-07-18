#include "bk_cuda.h"
#include "../utils.h"
#include <iostream>

// For the CUDA runtime routines (prefixed with "cuda_")
#include <cuda.h>
#include <cuda_runtime.h>
#include <nvrtc.h>

static const char* _cudaGetErrorEnum(cudaError_t error) {
	return cudaGetErrorName(error);
}
static const char* _cudaGetErrorEnum(CUresult error) {
	static char unknown[] = "<unknown>";
	const char* ret = NULL;
	cuGetErrorName(error, &ret);
	return ret ? ret : unknown;
}

template <typename T>
void check(T result, char const* const func, const char* const file,
	int const line) {
	if (result) {
		fprintf(stderr, "CUDA error at %s:%d code=%d(%s) \"%s\" \n", file, line, static_cast<unsigned int>(result), _cudaGetErrorEnum(result), func);
		cudaDeviceReset();
		BAIL
	}
}

#define checkCudaErrors(val) check((val), #val, __FILE__, __LINE__)

#define NVRTC_SAFE_CALL(Name, x)                                \
  do {                                                          \
    nvrtcResult result = x;                                     \
    if (result != NVRTC_SUCCESS) {                              \
      std::cerr << "\nerror: " << Name << " failed with error " \
                << nvrtcGetErrorString(result);                 \
      exit(1);                                                  \
    }                                                           \
  } while (0)



std::string compileToPTX(const std::string& file, const std::string& name) {
	int numCompileOptions = 0;

	// compile
	nvrtcProgram prog;
	NVRTC_SAFE_CALL("nvrtcCreateProgram", nvrtcCreateProgram(&prog, file.data(), name.data(), 0, NULL, NULL));

	nvrtcResult res = nvrtcCompileProgram(prog, 0, nullptr);

	// dump log
	size_t logSize;
	NVRTC_SAFE_CALL("nvrtcGetProgramLogSize", nvrtcGetProgramLogSize(prog, &logSize));
	char* log = reinterpret_cast<char*>(malloc(sizeof(char) * logSize + 1));
	NVRTC_SAFE_CALL("nvrtcGetProgramLog", nvrtcGetProgramLog(prog, log));
	log[logSize] = '\x0';

	if (strlen(log) >= 2) {
		std::cerr << "\n compilation log ---\n";
		std::cerr << log;
		std::cerr << "\n end log ---\n";
	}

	free(log);

	NVRTC_SAFE_CALL("nvrtcCompileProgram", res);

	// fetch PTX
	size_t ptxSize;
	NVRTC_SAFE_CALL("nvrtcGetPTXSize", nvrtcGetPTXSize(prog, &ptxSize));
	std::string ptx(ptxSize, 0);
	//char* ptx = reinterpret_cast<char*>(malloc(sizeof(char) * ptxSize));
	NVRTC_SAFE_CALL("nvrtcGetPTX", nvrtcGetPTX(prog, ptx.data()));
	std::cout << ptx;
	NVRTC_SAFE_CALL("nvrtcDestroyProgram", nvrtcDestroyProgram(&prog));
	return ptx;
}

CUmodule loadPTX(const std::string& ptx) {
	CUmodule module;
	checkCudaErrors(cuModuleLoadDataEx(&module, ptx.data(), 0, 0, 0));
	return module;
}

float* h_A, * h_B, * h_C;
CUdeviceptr d_A, d_B, d_C;
int numElements;
size_t size;
CUfunction kernel_addr;

int CUDA_init() {
	{
		int device_count = 0;
		cudaDeviceProp deviceProp;
		checkCudaErrors(cudaGetDeviceCount(&device_count));
		BAIL_IF(device_count, 0)
		int current_device = 0;
		while (current_device < device_count) {
			cudaGetDeviceProperties(&deviceProp, current_device);
			printf("GPU Device %d: \"%s\" with compute capability %d.%d\n\n", current_device, deviceProp.name, deviceProp.major, deviceProp.minor);
			current_device++;
		}
	}
	checkCudaErrors(cudaSetDevice(0));
	checkCudaErrors(cuInit(0));
	CUdevice cuDevice;
	CUcontext context;
	checkCudaErrors(cuDeviceGet(&cuDevice, 0));
	checkCudaErrors(cuCtxCreate(&context, 0, cuDevice));


	std::string kernel =
		R"(extern "C" __global__ void vectorAdd(const float* A, const float* B, float* C,
			int numElements) {
		int i = blockDim.x * blockIdx.x + threadIdx.x;

		if (i < numElements) {
			C[i] = A[i] + B[i];
		}
	})";

	const std::string ptx = compileToPTX(kernel, "vectorAddKernel");
	CUmodule module = loadPTX(ptx);


	checkCudaErrors(cuModuleGetFunction(&kernel_addr, module, "vectorAdd"));

	// Print the vector length to be used, and compute its size
	numElements = 50000;
	size = numElements * sizeof(float);
	printf("[Vector addition of %d elements]\n", numElements);

	// Allocate the host input vector A
	h_A = reinterpret_cast<float*>(malloc(size));
	// Allocate the host input vector B
	h_B = reinterpret_cast<float*>(malloc(size));
	// Allocate the host output vector C
	h_C = reinterpret_cast<float*>(malloc(size));

	// Verify that allocations succeeded
	if (h_A == NULL || h_B == NULL || h_C == NULL) {
		fprintf(stderr, "Failed to allocate host vectors!\n");
		exit(EXIT_FAILURE);
	}

	// Initialize the host input vectors
	for (int i = 0; i < numElements; ++i) {
		h_A[i] = rand() / static_cast<float>(RAND_MAX);
		h_B[i] = rand() / static_cast<float>(RAND_MAX);
	}

	// Allocate the device input vector A
	checkCudaErrors(cuMemAlloc(&d_A, size));

	// Allocate the device input vector B
	checkCudaErrors(cuMemAlloc(&d_B, size));

	// Allocate the device output vector C
	checkCudaErrors(cuMemAlloc(&d_C, size));

	// Copy the host input vectors A and B in host memory to the device input
	// vectors in device memory
	printf("Copy input data from the host memory to the CUDA device\n");
	checkCudaErrors(cuMemcpyHtoD(d_A, h_A, size));
	checkCudaErrors(cuMemcpyHtoD(d_B, h_B, size));

	return 0;
}


void CUDA_go(size_t runs) {
	// Launch the Vector Add CUDA Kernel
	int threadsPerBlock = 256;
	int blocksPerGrid = (numElements + threadsPerBlock - 1) / threadsPerBlock;
	printf("CUDA kernel launch with %d blocks of %d threads\n", blocksPerGrid, threadsPerBlock);
	dim3 cudaBlockSize(threadsPerBlock, 1, 1);
	dim3 cudaGridSize(blocksPerGrid, 1, 1);

	void* arr[] = { reinterpret_cast<void*>(&d_A), reinterpret_cast<void*>(&d_B),
				   reinterpret_cast<void*>(&d_C),
				   reinterpret_cast<void*>(&numElements) };
	checkCudaErrors(cuLaunchKernel(kernel_addr, cudaGridSize.x, cudaGridSize.y,
		cudaGridSize.z, /* grid dim */
		cudaBlockSize.x, cudaBlockSize.y,
		cudaBlockSize.z, /* block dim */
		0, 0,            /* shared mem, stream */
		&arr[0],         /* arguments */
		0));
	checkCudaErrors(cuCtxSynchronize());

	// Copy the device result vector in device memory to the host result vector in host memory.
	printf("Copy output data from the CUDA device to the host memory\n");
	checkCudaErrors(cuMemcpyDtoH(h_C, d_C, size));

	// Verify that the result vector is correct
	for (int i = 0; i < numElements; ++i) {
		if (fabs(h_A[i] + h_B[i] - h_C[i]) > 1e-5) {
			fprintf(stderr, "Result verification failed at element %d!\n", i);
			exit(EXIT_FAILURE);
		}
	}

	printf("Test PASSED\n");


	printf("Done\n");
}


int CUDA_deInit() {

	// Free device global memory
	checkCudaErrors(cuMemFree(d_A));
	checkCudaErrors(cuMemFree(d_B));
	checkCudaErrors(cuMemFree(d_C));

	// Free host memory
	free(h_A);
	free(h_B);
	free(h_C);
	return 0;

}
