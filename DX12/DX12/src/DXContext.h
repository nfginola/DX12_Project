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
		uint64_t cbv_srv_uav = -1;
		uint64_t sampler = -1;
		uint64_t rtv = -1;
		uint64_t dsv = -1;

		void init(ID3D12Device* dev);
	};

public:
	DXContext(DXContext::Settings& settings);
	~DXContext();

	const HandleSizes& get_hdl_sizes();
	DXSwapChain* get_sc();

private:
	cptr<IDXGIAdapter> m_adapter;
	cptr<ID3D12Device> m_dev;
	cptr<ID3D12CommandQueue> m_prim_dq;		// primary dq

	HandleSizes m_hdl_sizes;

	uptr<DXSwapChain> m_sc;
	
};

