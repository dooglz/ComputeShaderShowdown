#include "../utils.h"
#include <wrl.h>


//#define WIN32_LEAN_AND_MEAN
//#include <Windows.h>

#include <combaseapi.h>
// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>

// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
#define ThrowIfFailed(hr) if (FAILED(hr)){BAIL}
/*inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr)){BAIL}
}*/


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

ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp);
ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter);
ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);


struct DxInfo {
	ComPtr<ID3D12Device2> device;
	// Compute objects.
	ComPtr<ID3D12CommandQueue> m_computeCommandQueue;
	ComPtr<ID3D12CommandAllocator> m_computeAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;
	ComPtr<ID3D12RootSignature> m_computeRootSignature;
	ComPtr<ID3D12PipelineState> m_computeState;
	//data objects
	ComPtr<ID3D12DescriptorHeap> CbvDHeap;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvGpuHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE uavCpuHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE uavGpuHandle;
	//CD3DX12_GPU_DESCRIPTOR_HANDLE cbvGpuHandle_dataIn;
	ComPtr<ID3D12Resource> computeConstantUploadBuffer;
	ComPtr<ID3D12Resource> computeConstantBuffer;
	ComPtr<ID3D12Resource> computeUAVBuffer;

	// Synchronization objects.
	ComPtr<ID3D12Fence>	m_computeFence;
	UINT64 m_computeFenceValue;
};

