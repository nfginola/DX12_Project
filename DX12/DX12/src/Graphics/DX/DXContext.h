#pragma once
#include <d3d12.h>
#include <dxgi.h>

class DXSwapChain;

class DXContext
{
public:
	struct Settings
	{
		HWND hwnd = nullptr;
		bool debug_on = false;
		UINT max_FIF = 3;
	};

	// Device-based constants
	struct HandleSizes
	{
		UINT cbv_srv_uav = -1;
		UINT sampler = -1;
		UINT rtv = -1;
		UINT dsv = -1;

		void init(ID3D12Device* dev);
	};

public:
	DXContext(DXContext::Settings& settings);
	~DXContext();

	const HandleSizes& get_hdl_sizes();
	DXSwapChain* get_sc();
	ID3D12Device* get_dev();
	ID3D12CommandQueue* get_direct_queue();
	ID3D12CommandQueue* get_copy_queue();
	ID3D12CommandQueue* get_compute_queue();

	uint64_t get_next_fence_value();

private:
	struct FinalDebug
	{
		~FinalDebug();
	};

private:
	Microsoft::WRL::ComPtr<ID3D12Device> m_dev;
	FinalDebug m_debug_print;		// Utilizing reverse destruction order to print remaining unreleased resources (only Device should be left, which we need or debug printing, but will be released right after)

	Microsoft::WRL::ComPtr<IDXGIAdapter> m_adapter;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_direct_queue, m_compute_queue, m_copy_queue;

	HandleSizes m_hdl_sizes;
	uptr<DXSwapChain> m_sc;
	uint64_t m_running_fence_value = 1;
	
};

