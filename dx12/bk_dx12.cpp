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

// Compute root signature parameter offsets.
enum ComputeRootParameters
{
	ComputeRootCBV = 0,
	ComputeRootUAVTable,
	ComputeRootParametersCount
};
const int32_t bufferLength = 16384;
const uint32_t bufferSize = sizeof(int32_t) * bufferLength;
const uint32_t alignedBufferSize = AlignUp(bufferSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

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
		//check 1.1 supported.
		ThrowIfFailed(dxInfo->device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))

			//CbvSrvUavTable
			CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		//Data in, Constant buffer view (CBV). Static - so can't be changed while shader is on a command list/bundle
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		//Data out. Data is volatile and can be changes, exept when while shader is on a command list/bundle.
		//if broke, try RANGE_FLAG_DATA_VOLATILE, which would mean it can always be changed (more work for driver)
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);


		CD3DX12_ROOT_PARAMETER1 computeRootParameters[ComputeRootParametersCount];
		computeRootParameters[ComputeRootCBV].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
		computeRootParameters[ComputeRootUAVTable].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
		computeRootSignatureDesc.Init_1_1(ComputeRootParametersCount, computeRootParameters, 0, nullptr, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;

		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(dxInfo->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&dxInfo->m_computeRootSignature)));
		NAME_D3D12_OBJECT(dxInfo->m_computeRootSignature);

	}
	// Create a descriptor heap
	{

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = 2;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(dxInfo->device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&dxInfo->CbvDHeap)));
		dxInfo->CbvDHeap->SetName(L"Constant Buffer View Descriptor Heap");
	}
	D3D12_RESOURCE_ALLOCATION_INFO rai;
	rai.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	rai.SizeInBytes = alignedBufferSize;

	// Create the constant buffer.
	{
		ThrowIfFailed(dxInfo->device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(rai),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&dxInfo->computeUploadBuffer)));

		ThrowIfFailed(dxInfo->device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(rai),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&dxInfo->computeReadbackBuffer)));



		// Describe and create a constant buffer view.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc[2];// = {};
		cbvDesc[0].BufferLocation = dxInfo->computeUploadBuffer->GetGPUVirtualAddress();
		cbvDesc[0].SizeInBytes = alignedBufferSize;

		dxInfo->cbvCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(dxInfo->CbvDHeap->GetCPUDescriptorHandleForHeapStart(), 0, 0);
		dxInfo->cbvGpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(dxInfo->CbvDHeap->GetGPUDescriptorHandleForHeapStart(), 0, 0);
		dxInfo->device->CreateConstantBufferView(cbvDesc, dxInfo->cbvCpuHandle);
	}
	//Upload Data to CB
	{
		int32_t* payload;
		// Initialize and map the constant buffers. We don't unmap this until the
		// app closes. Keeping things mapped for the lifetime of the resource is okay.
		ThrowIfFailed(dxInfo->computeUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&payload)));
		//memcpy(mMappedWVPBuffer, &mWVPData, sizeof(mWVPData));
		for (uint32_t k = 1; k < bufferLength; k++) {
			payload[k] = (k % 10);//rand();
		}
		dxInfo->computeUploadBuffer->Unmap(0, nullptr);
	}

	//Create UAV
	{
		//CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle0(CbvDHeap->GetCPUDescriptorHandleForHeapStart(), 0, m_srvUavDescriptorSize);


		ThrowIfFailed(dxInfo->device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(rai, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&dxInfo->computeUAVBuffer)));

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = bufferLength;
		uavDesc.Buffer.StructureByteStride = sizeof(int32_t);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		UINT m_srvUavDescriptorSize = dxInfo->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		dxInfo->uavCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(dxInfo->CbvDHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_srvUavDescriptorSize);
		dxInfo->uavGpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(dxInfo->CbvDHeap->GetGPUDescriptorHandleForHeapStart(), 1, m_srvUavDescriptorSize);
		dxInfo->device->CreateUnorderedAccessView(dxInfo->computeUAVBuffer.Get(), nullptr, &uavDesc, dxInfo->uavCpuHandle);

	}
	//Create command lists and allocators
	{
		// Create the command list.
		ThrowIfFailed(dxInfo->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&dxInfo->directAllocator)));
		ThrowIfFailed(dxInfo->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, dxInfo->directAllocator.Get(), nullptr, IID_PPV_ARGS(&dxInfo->directCommandList)));
		dxInfo->directCommandQueue = CreateCommandQueue(dxInfo->device, D3D12_COMMAND_LIST_TYPE_DIRECT);
		NAME_D3D12_OBJECT(dxInfo->directCommandList);
		ThrowIfFailed(dxInfo->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&dxInfo->m_computeAllocator)));
		ThrowIfFailed(dxInfo->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, dxInfo->m_computeAllocator.Get(), nullptr, IID_PPV_ARGS(&dxInfo->m_computeCommandList)));
		ThrowIfFailed(dxInfo->device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&dxInfo->m_computeFence)));
		dxInfo->m_computeFenceValue = 0;
		SetName(dxInfo->m_computeCommandList.Get(), L"m_computeCommandLists (compute queue)");
		SetName(dxInfo->m_computeAllocator.Get(), L"m_computeAllocator");
		dxInfo->m_computeCommandList->Close();
	}
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
			"#define bufferLength " + std::to_string(bufferLength) + "\n"
			"struct Foo{int a;};\n"
			"int dataIn[bufferLength]: register(b0);\n"
			//"int dataOut[100]: register(u0);\n"
			//"ConstantBuffer<Foo> dataIn : register(b0);//constbuf\n"
			//"RWStructuredBuffer<int> dataIn : register(b0);// UAV\n"
			"RWStructuredBuffer<int> dataOut : register(u0);// UAV\n"
			"[numthreads(blocksize, 1, 1)]\n"
			"void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex){\n"
			"if(DTid.x < bufferLength){\n"
			"const int i = dataIn[DTid.x];\n"
			//" dataOut[0] = i;\n"
			"dataOut[DTid.x] = i*2;\n"
			//"dataIn[0] = i;\n"
			"}\n"
			"}";

		ComPtr<ID3DBlob> errors = nullptr;
		const auto result = D3DCompile(shaderString.data(), shaderString.size(), "SimpleShader", nullptr, nullptr, "CSMain", "cs_5_1", compileFlags, 0, &computeShader, &errors);

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
		//D3DCompileFromFile("", nullptr, nullptr, "CSMain", "cs_5_1", compileFlags, 0, &computeShader, nullptr));

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

	ID3D12DescriptorHeap* ppHeaps[] = { dxInfo->CbvDHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);


	//pCommandList->SetComputeRootConstantBufferView(ComputeRootCBV, dxInfo->computeConstantBuffer->GetGPUVirtualAddress());
	pCommandList->SetComputeRootDescriptorTable(ComputeRootCBV, dxInfo->cbvGpuHandle);
	pCommandList->SetComputeRootDescriptorTable(ComputeRootUAVTable, dxInfo->uavGpuHandle);
	pCommandList->Dispatch(512, 1, 1);

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

	//Upload Data back
	{

		//	UpdateSubresources<1>(dxInfo->directCommandList.Get(), dxInfo->computeConstantUploadBuffer.Get(), dxInfo->computeUAVBuffer.Get(), 0, 0, 1, &computeCBData);
			//dxInfo->directCommandList->CopyBufferRegion(dxInfo->computeConstantUploadBuffer.Get(), 0, pIntermediate, pLayouts[0].Offset, pLayouts[0].Footprint.Width);
		dxInfo->directCommandList->CopyResource(dxInfo->computeReadbackBuffer.Get(), dxInfo->computeUAVBuffer.Get());
		ThrowIfFailed(dxInfo->directCommandList->Close());

		// Execute the command list.
		ID3D12CommandList* ppCommandLists[] = { dxInfo->directCommandList.Get() };
		dxInfo->directCommandQueue->ExecuteCommandLists(1, ppCommandLists);
		dxInfo->m_computeFenceValue++;
		ThrowIfFailed(dxInfo->directCommandQueue->Signal(dxInfo->m_computeFence.Get(), dxInfo->m_computeFenceValue));
		WaitForFenceValue(dxInfo->m_computeFence.Get(), dxInfo->m_computeFenceValue);

		//CD3DX12_RESOURCE_BARRIER
	//	pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
	//		dxInfo->computeUAVBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ
	//	));

		int32_t* payload;
		// Initialize and map the constant buffers. We don't unmap this until the
		// app closes. Keeping things mapped for the lifetime of the resource is okay.

		//just having nullptr to denote 'whole buffer' get complaints form validation layer.
		D3D12_RANGE rr = { 0,alignedBufferSize };
		ThrowIfFailed(dxInfo->computeReadbackBuffer->Map(0, &rr, reinterpret_cast<void**>(&payload)));
		//memcpy(mMappedWVPBuffer, &mWVPData, sizeof(mWVPData));
		for (uint32_t k = 1; k < bufferLength; k++) {
			//	payload[k] = (k % 10);//rand();
		}
		dxInfo->computeReadbackBuffer->Unmap(0, nullptr);
	}

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

