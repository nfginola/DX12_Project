#pragma once

#include <string>

#include <d3d12.h>
#include <dxgi1_4.h>		// For CreateDXGIFactory2
#include <dxgi1_6.h>		// Querying high perf GPU (e.g dGPU instead of iGPU on laptop)
#include <dxgidebug.h>		// DXGI debug device (requires linking to dxiguid.lib)

#include "d3dx12.h"

void ThrowIfFailed(HRESULT hr, const std::string& errMsg);

enum class RootArgDest
{
	eInvalid,
	eGraphics,
	eCompute
};

enum class UsageIntentCPU
{
	eInvalid,

	eUpdateNever,					// Maps to device-local memory
	eUpdateSometimes,				// Maps to device-local memory
	eUpdateOnce,					// Upload if GPU intends to read once OR device-local if GPU intends to read many times   (transient)
	eUpdateMultipleTimesPerFrame,	// Upload if GPU intends to read onece OR device-local if GPU intends to read many times  (transient)
};

enum class UsageIntentGPU
{
	eInvalid,

	eReadOncePerFrame,
	eReadMultipleTimesPerFrame,
	eWrite
};

enum class BufferFlag
{
	eInvalid,
	eConstant,
	eNonConstant
};

enum class TextureFlag
{
	eInvalid,
	eNonSRGB,
	eSRGB
};

enum class ShaderModel
{
	e6_0,
	e6_6
};

enum class ShaderType
{
	eVertex,
	ePixel,
	eCompute,
	eHull,
	eDomain,
	eGeometry
};

enum class DepthFormat 
{ 
	eD32, 
	eD32_S8, 
	eD24_S8, 
	eD16
};


struct DXFence
{
public:
	DXFence() = default;
	DXFence(ID3D12Device* dev)
	{
		m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		auto hr = dev->CreateFence(m_fence_val_to_wait_for, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf()));
		if (FAILED(hr))
			assert(false);
	}

	void signal(ID3D12CommandQueue* queue, UINT sig_val)
	{
		m_fence_val_to_wait_for = sig_val;
		queue->Signal(
			m_fence.Get(),
			m_fence_val_to_wait_for);
	}

	// GPU wait
	void wait(ID3D12CommandQueue* queue)
	{
		queue->Wait(m_fence.Get(), m_fence_val_to_wait_for);
	}

	// CPU wait
	void wait() const
	{
		if (m_fence->GetCompletedValue() < m_fence_val_to_wait_for)
		{
			// Raise an event when fence reaches fenceVal
			ThrowIfFailed(m_fence->SetEventOnCompletion(m_fence_val_to_wait_for, m_fence_event), DET_ERR("Failed to couple Fence and Event"));
			// CPU block until event is done
			WaitForSingleObject(m_fence_event, INFINITE);
		}
	}

private:
	UINT m_fence_val_to_wait_for = 0;
	cptr<ID3D12Fence> m_fence;
	HANDLE m_fence_event{};
};

