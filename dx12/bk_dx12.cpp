#include "bk_dx12.h"
#include "../utils.h"
#include "bk_dx12_utils.h"

#include <iostream>
#include <chrono>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h> // For CommandLineToArgvW

// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

// In order to define a function called CreateWindow, the Windows macro needs to be undefined.
#if defined(CreateWindow)
#undef CreateWindow
#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

//#include <combaseapi.h>
// D3D12 extension library.
#include "d3dx12.h"

std::shared_ptr <DxInfo> dxInfo;

int DX12_init() {
	dxInfo = std::make_shared<DxInfo>();

#if defined(_DEBUG)
	// Always enable the debug layer before doing anything DX12 related so all possible errors generated while creating DX12 objects are caught by the debug layer.
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif

	ComPtr<IDXGIAdapter4>  adapter = GetAdapter(false);
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
		adapter->GetDesc1(&dxgiAdapterDesc1);
		std::cout << "Using Adapter:" << dxgiAdapterDesc1.DeviceId << std::endl;
	}
	dxInfo->device = CreateDevice(adapter);
	dxInfo->m_computeCommandQueue = CreateCommandQueue(dxInfo->device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	NAME_D3D12_OBJECT(dxInfo->m_computeCommandQueue);
	std::cout << "Created Cmd Queue: " << dxInfo->m_computeCommandQueue << std::endl;

	{
		//root sigs
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(dxInfo->device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		// Create compute signature.
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);


		// Compute root signature parameter offsets.
		enum ComputeRootParameters
		{
			SrvUavTable,
			RootConstants, // Root constants that give the shader information about the triangle vertices and culling planes.
			ComputeRootParametersCount
		};

		CD3DX12_ROOT_PARAMETER1 computeRootParameters[ComputeRootParametersCount];
		computeRootParameters[SrvUavTable].InitAsDescriptorTable(2, ranges);
		computeRootParameters[RootConstants].InitAsConstants(4, 0);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
		computeRootSignatureDesc.Init_1_1(_countof(computeRootParameters), computeRootParameters);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;

		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(dxInfo->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&dxInfo->m_computeRootSignature)));
		NAME_D3D12_OBJECT(dxInfo->m_computeRootSignature);

	}


	ThrowIfFailed(dxInfo->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&dxInfo->m_computeAllocator)));
	ThrowIfFailed(dxInfo->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, dxInfo->m_computeAllocator.Get(), nullptr, IID_PPV_ARGS(&dxInfo->m_computeCommandList)));
	ThrowIfFailed(dxInfo->device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&dxInfo->m_computeFence)));
	dxInfo->m_computeFenceValue = 0;
	SetName(dxInfo->m_computeCommandList.Get(), L"m_computeCommandLists (compute queue)");
	SetName(dxInfo->m_computeAllocator.Get(), L"m_computeAllocator");
	dxInfo->m_computeCommandList->Close();

	// Create the pipeline states, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> computeShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		const std::string shaderString =
			"#define blocksize 256\n"
			"[numthreads(blocksize, 1, 1)]"
			"void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex){"
			"}";

		ComPtr<ID3DBlob> errors = nullptr;
		const auto result = D3DCompile(shaderString.data(), shaderString.size(), "SimpleShader", nullptr, nullptr, "CSMain", "cs_5_0", compileFlags, 0, &computeShader, &errors);

		if (FAILED(result)) {
			if (errors) {
				std::shared_ptr<char[]> ebuf(new char[errors->GetBufferSize()]);
				std::memcpy(ebuf.get(), errors->GetBufferPointer(), errors->GetBufferSize());
				std::cerr << "DX shader compile error " << ebuf << std::endl;
			}
			else {
				std::cerr << "DX shader compile issue, no errors reported " << std::endl;
			}
			BAIL
		}
		std::cout << "Shader compiled, Size: " << computeShader->GetBufferSize() << std::endl;
		//D3DCompileFromFile("", nullptr, nullptr, "CSMain", "cs_5_0", compileFlags, 0, &computeShader, nullptr));

		// Describe and create the compute pipeline state object (PSO).
		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = dxInfo->m_computeRootSignature.Get();
		computePsoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());

		ThrowIfFailed(dxInfo->device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&dxInfo->m_computeState)));
		NAME_D3D12_OBJECT(dxInfo->m_computeState);
	}

	return 0;
}


std::chrono::nanoseconds WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue,
	std::chrono::milliseconds duration = std::chrono::milliseconds::max())
{
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	ASSERT_BAIL(fenceEvent);
	const auto start = std::chrono::high_resolution_clock::now();
	if (fence->GetCompletedValue() < fenceValue)
	{
		ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
		WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
	}
	return std::chrono::high_resolution_clock::now() - start;
}


void DX12_go(size_t runs)
{
	// Run the particle simulation using the compute shader.
	ID3D12GraphicsCommandList* pCommandList = dxInfo->m_computeCommandList.Get();
	ID3D12CommandAllocator* pCommandAllocator = dxInfo->m_computeAllocator.Get();
	//PIXScopedEvent(pCommandList, 0, "Simulation step");
	ThrowIfFailed(pCommandAllocator->Reset());
	ThrowIfFailed(pCommandList->Reset(pCommandAllocator, dxInfo->m_computeState.Get()));
	pCommandList->SetPipelineState(dxInfo->m_computeState.Get());
	pCommandList->SetComputeRootSignature(dxInfo->m_computeRootSignature.Get());
	pCommandList->Dispatch(1, 1, 1);

	//pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pUavResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

		// Close and execute the command list.
	ThrowIfFailed(pCommandList->Close());

	ID3D12CommandList* ppCommandLists[] = { pCommandList };

	for (size_t i = 0; i < runs; i++)
	{
		std::cout << "Submit\n";
		auto t1 = std::chrono::high_resolution_clock::now();

		dxInfo->m_computeCommandQueue->ExecuteCommandLists(1, { ppCommandLists });
		dxInfo->m_computeFenceValue++;
		dxInfo->m_computeCommandQueue->Signal(dxInfo->m_computeFence.Get(), dxInfo->m_computeFenceValue);

		WaitForFenceValue(dxInfo->m_computeFence.Get(), dxInfo->m_computeFenceValue);
		auto t2 = std::chrono::high_resolution_clock::now();
		std::cout << "Done " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() << "ns \n";
	}

	//m_computeFenceValues[m_frameIndex] = m_computeFenceValue;
	//m_computeCommandQueue->Signal(m_computeFences[m_frameIndex].Get(), m_computeFenceValue);
	//PIXEndEvent(m_computeCommandQueue.Get());
	// Wait for compute fence to finish

}
// this will only call release if an object exists (prevents exceptions calling release on non existant objects)
#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = nullptr; } }
int DX12_deInit()
{

	dxInfo.reset();
	/*
	SAFE_RELEASE(m_computeFence);

	ThrowIfFailed(m_computeAllocator->Reset());
	ThrowIfFailed(m_computeCommandList->Reset(m_computeAllocator.Get() , m_computeState.Get()));
	ThrowIfFailed(m_computeCommandList->Close());

	SAFE_RELEASE(m_computeCommandList);

	SAFE_RELEASE(m_computeState);

	SAFE_RELEASE(m_computeAllocator);

	SAFE_RELEASE(m_computeCommandList);
	SAFE_RELEASE(m_computeCommandQueue);

	SAFE_RELEASE(m_computeRootSignature);
	SAFE_RELEASE(device);
	return 0;
	*/
	return 0;
}

