#pragma once

#include "DXCommon.h"



class DXSwapChain
{
public:
	struct Settings
	{
		HWND hwnd;
		uint8_t max_FIF = 3;
		cptr<ID3D12CommandQueue> associated_queue;

		// 0 is valid: It will grab client dimensions from associated window automatically
		uint64_t width = 0;
		uint64_t height = 0;
	};

public:
	DXSwapChain(DXSwapChain::Settings& settings, IDXGIFactory4* fac);
	~DXSwapChain() = default;

	void present(bool vsync, UINT flags = 0);
	UINT get_curr_draw_surface() const;

	ID3D12Resource* get_backbuffer(UINT index) const;
	const DXSwapChain::Settings& get_settings() const;

private:
	Microsoft::WRL::ComPtr<IDXGISwapChain3> m_sc3;
	std::vector<cptr<ID3D12Resource>> m_backbuffers;
	Settings m_settings;

	bool m_tearing_supported = false;
};

