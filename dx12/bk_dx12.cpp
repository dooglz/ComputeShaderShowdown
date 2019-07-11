#include "bk_dx12.h"
#include "../utils.h"
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

// From DXSampleHelper.h 
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		BAIL
	}
}


// Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
	pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
	WCHAR fullName[50];
	if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
	{
		pObject->SetName(fullName);
	}
}
#else
inline void SetName(ID3D12Object*, LPCWSTR)
{
}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}
#endif


// Naming helper for ComPtr<T>.
// Assigns the name of the variable as the name of the object.
// The indexed variant will include the index in the name of the object.
#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp)
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));
	ComPtr<IDXGIAdapter1> dxgiAdapter1;
	ComPtr<IDXGIAdapter4> dxgiAdapter4;

	if (useWarp)
	{
		ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
		ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
	}
	else
	{
		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			std::wcout << "Adapter " << dxgiAdapterDesc1.DeviceId << " - " << dxgiAdapterDesc1.Description << ", FB: " << dxgiAdapterDesc1.DedicatedVideoMemory << std::endl;
			// Check to see if the adapter can create a D3D12 device without actually 
			// creating it. The adapter with the largest dedicated video memory is favored.
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
			}
		}
	}
	DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
	dxgiAdapter4->GetDesc1(&dxgiAdapterDesc1);
	std::cout << "Using Adapter:" << dxgiAdapterDesc1.DeviceId << std::endl;

	return dxgiAdapter4;
}
ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
	ComPtr<ID3D12Device2> d3d12Device2;
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));
	HRESULT WINAPI D3D12CreateDevice(
		_In_opt_  IUnknown * pAdapter,
		D3D_FEATURE_LEVEL MinimumFeatureLevel,
		_In_      REFIID            riid,
		_Out_opt_ void** ppDevice
	);
	// Enable debug messages in debug mode.
#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
		// Suppress whole categories of messages
	  //D3D12_MESSAGE_CATEGORY Categories[] = {};

	  // Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
	}
#endif

	return d3d12Device2;
}

ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandQueue> d3d12CommandQueue;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));

	return d3d12CommandQueue;
}

// Compute objects.
ComPtr<ID3D12CommandQueue> m_computeCommandQueue;
ComPtr<ID3D12CommandAllocator> m_computeAllocator;
ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;
ComPtr<ID3D12RootSignature> m_computeRootSignature;
ComPtr<ID3D12PipelineState> m_computeState;
// Synchronization objects.
ComPtr<ID3D12Fence>	m_computeFence;
UINT64 m_computeFenceValue;

int DX12_init() {

	// The number of swap chain back buffers.
	const uint8_t g_NumFrames = 3;
	// Use WARP adapter
	bool g_UseWarp = false;

	uint32_t g_ClientWidth = 1280;
	uint32_t g_ClientHeight = 720;

	// Set to true once the DX12 objects have been initialized.
	bool g_IsInitialized = false;
	// Window handle.
	HWND g_hWnd;
	// Window rectangle (used to toggle fullscreen state).
	RECT g_WindowRect;

	// DirectX 12 Objects
	ComPtr<ID3D12Device2> g_Device;
	ComPtr<ID3D12CommandQueue> g_CommandQueue;
	ComPtr<IDXGISwapChain4> g_SwapChain;
	ComPtr<ID3D12Resource> g_BackBuffers[g_NumFrames];
	ComPtr<ID3D12GraphicsCommandList> g_CommandList;
	ComPtr<ID3D12CommandAllocator> g_CommandAllocators[g_NumFrames];
	ComPtr<ID3D12DescriptorHeap> g_RTVDescriptorHeap;
	UINT g_RTVDescriptorSize;
	UINT g_CurrentBackBufferIndex;

#if defined(_DEBUG)
	// Always enable the debug layer before doing anything DX12 related so all possible errors generated while creating DX12 objects are caught by the debug layer.
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif

	ComPtr<IDXGIAdapter4>  adapter = GetAdapter(false);

	ComPtr<ID3D12Device2> device = CreateDevice(adapter);
	m_computeCommandQueue = CreateCommandQueue(device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	NAME_D3D12_OBJECT(m_computeCommandQueue);
	std::cout << "Created Cmd Queue: " << m_computeCommandQueue << std::endl;




	{
		//root sigs
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
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
		ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_computeRootSignature)));
		NAME_D3D12_OBJECT(m_computeRootSignature);

	}


	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_computeAllocator)));
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_computeAllocator.Get(), nullptr, IID_PPV_ARGS(&m_computeCommandList)));
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_computeFence)));
	m_computeFenceValue = 0;
	SetName(m_computeCommandList.Get(), L"m_computeCommandLists (compute queue)");
	SetName(m_computeAllocator.Get(), L"m_computeAllocator");
	m_computeCommandList->Close();

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
			if(errors){
				std::shared_ptr<char[]> ebuf(new char[errors->GetBufferSize()]);
				std::memcpy(ebuf.get(), errors->GetBufferPointer(), errors->GetBufferSize());
				std::cerr << "DX shader compile error " << ebuf << std::endl;
			}else{
				std::cerr << "DX shader compile issue, no errors reported "<< std::endl;
			}
			BAIL
		}
		std::cout << "Shader compiled, Size: " << computeShader->GetBufferSize() << std::endl;
		//D3DCompileFromFile("", nullptr, nullptr, "CSMain", "cs_5_0", compileFlags, 0, &computeShader, nullptr));

		// Describe and create the compute pipeline state object (PSO).
		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = m_computeRootSignature.Get();
		computePsoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());

		ThrowIfFailed(device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_computeState)));
		NAME_D3D12_OBJECT(m_computeState);
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
	ID3D12GraphicsCommandList* pCommandList = m_computeCommandList.Get();
	ID3D12CommandAllocator* pCommandAllocator = m_computeAllocator.Get();
	//PIXScopedEvent(pCommandList, 0, "Simulation step");
	ThrowIfFailed(pCommandAllocator->Reset());
	ThrowIfFailed(pCommandList->Reset(pCommandAllocator, m_computeState.Get()));
	pCommandList->SetPipelineState(m_computeState.Get());
	pCommandList->SetComputeRootSignature(m_computeRootSignature.Get());
	pCommandList->Dispatch(1, 1, 1);

	//pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pUavResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));


	ThrowIfFailed(pCommandList->Close());
	// Close and execute the command list.
	ID3D12CommandList* ppCommandLists[] = { pCommandList };
	std::cout << "Dispatch" << std::endl;
	m_computeCommandQueue->ExecuteCommandLists(1, { ppCommandLists });
	m_computeFenceValue++;
	m_computeCommandQueue->Signal(m_computeFence.Get(), m_computeFenceValue);
	
	WaitForFenceValue(m_computeFence.Get(), m_computeFenceValue);
	std::cout << "Done" << std::endl;
	//m_computeFenceValues[m_frameIndex] = m_computeFenceValue;
	//m_computeCommandQueue->Signal(m_computeFences[m_frameIndex].Get(), m_computeFenceValue);
	//PIXEndEvent(m_computeCommandQueue.Get());

	// Wait for compute fence to finish

}

int DX12_deInit()
{
	return 0;
}

