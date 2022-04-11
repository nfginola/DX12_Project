#pragma once
#include <d3d12.h>
#include <dxgi.h>

class DXSwapChain;

class DXContext
{
public:
	struct Settings
	{
		HWND hwnd;					// Associated window (optional)
		bool debug_on = false;
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

private:
	cptr<IDXGIAdapter> m_adapter;
	cptr<ID3D12Device> m_dev;
	cptr<ID3D12CommandQueue> m_direct_queue;

	HandleSizes m_hdl_sizes;

	uptr<DXSwapChain> m_sc;
	
};

