#include "pch.h"
#include "DXSwapChain.h"

bool is_tearing_supported();
void validate_settings(DXSwapChain::Settings& settings);

DXSwapChain::DXSwapChain(DXSwapChain::Settings& settings, IDXGIFactory4* fac) :
	m_settings(settings),
	m_tearing_supported(is_tearing_supported())
{
	validate_settings(settings);
	
	// create swapchain
	DXGI_SWAP_CHAIN_DESC1 scDesc{};
	scDesc.Width = (UINT)settings.width;
	scDesc.Height = (UINT)settings.height;
	scDesc.BufferCount = settings.max_FIF;
	scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.SampleDesc.Count = 1;
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.Flags = is_tearing_supported() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	// https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforhwnd
	// https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/D3D12HelloTriangle.cpp#L95 (ref)
	cptr<IDXGISwapChain1> sc;
	ThrowIfFailed(fac->CreateSwapChainForHwnd(
		settings.associated_queue.Get(),					// swap chain needs the queue (specifically Direct) so it has the ability to "force a flush" on it.		--> Presumably CPU waits on GPU here, for example in the case of vsync
		settings.hwnd,
		&scDesc,
		nullptr,		// no fullscreen
		nullptr,		// no output restrictions
		sc.GetAddressOf()), "Failed to create swapchain");

	// Disable alt enter fullscreen (no fs support)
	ThrowIfFailed(fac->MakeWindowAssociation(settings.hwnd, DXGI_MWA_NO_ALT_ENTER), "Disabling alt enter failed");

	// Grab swapchain3 so that we have access to GetBackBufferIndex (which surface index to draw on next)
	sc.As(&m_sc3);
	
	// Grab backbuffers
	m_backbuffers.resize(settings.max_FIF);
	for (auto i = 0; i < m_backbuffers.size(); ++i)
		ThrowIfFailed(m_sc3->GetBuffer(i, IID_PPV_ARGS(m_backbuffers[i].GetAddressOf())), DET_ERR("Failed to get back buffer"));

	// Update app client dimensions
	auto bb_desc = m_backbuffers[0]->GetDesc();
	m_settings.width = bb_desc.Width;
	m_settings.height = bb_desc.Height;
}

void DXSwapChain::present(bool vsync, UINT flags)
{
	if (!vsync && m_tearing_supported)
		flags |= DXGI_PRESENT_ALLOW_TEARING;

	ThrowIfFailed(m_sc3->Present(vsync ? 1 : 0, flags), DET_ERR("Could not present to swapchain"));
}

UINT DXSwapChain::get_curr_draw_surface() const
{
	return m_sc3->GetCurrentBackBufferIndex();
}

ID3D12Resource* DXSwapChain::get_backbuffer(UINT index) const
{
	return m_backbuffers[index].Get();
}

const DXSwapChain::Settings& DXSwapChain::get_settings() const
{
	return m_settings;
}







bool is_tearing_supported()
{
	BOOL allowed = FALSE;
	cptr<IDXGIFactory4> fac4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&fac4))))
	{
		cptr<IDXGIFactory5> fac5;
		if (SUCCEEDED(fac4.As(&fac5)))
		{
			auto hr = fac5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowed, sizeof(allowed));

			allowed = SUCCEEDED(hr) && allowed;
		}
	}
	return allowed;
}

void validate_settings(DXSwapChain::Settings& settings)
{
	if (!settings.hwnd)
		throw std::runtime_error(DET_ERR("Swap chain requires an associated window!"));

	// verify queue type
	auto d = settings.associated_queue->GetDesc();
	if (d.Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
		throw std::runtime_error(DET_ERR("Swap chain requires a Direct Queue!"));
}
