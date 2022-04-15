#include "pch.h"
#include "DXContext.h"

#include "DXCommon.h"
#include "DXSwapChain.h"

cptr<IDXGIAdapter> select_adapter(IDXGIFactory6* fac);
void set_info_queue_prefs(ID3D12Device* dev);
void validate_settings(DXContext::Settings& settings);
void append_debug_info_to_title(HWND& hwnd, bool debug_on);

DXContext::DXContext(DXContext::Settings& settings)
{
	constexpr uint64_t MAX_FIF = 3;

	validate_settings(settings);
	append_debug_info_to_title(settings.hwnd, settings.debug_on);

	UINT factory_flags = 0;
	if (settings.debug_on)
	{
		cptr<ID3D12Debug1> debug;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(debug.GetAddressOf())), DET_ERR("Failed to get the debug interface"));
		debug->EnableDebugLayer();
		debug->SetEnableGPUBasedValidation(true);

		factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
	}
	
	// Grab fac6 for GPU perf preference on enumeration
	cptr<IDXGIFactory2> fac;
	ThrowIfFailed(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(fac.GetAddressOf())), DET_ERR("Failed to create DXGIFactory2"));
	cptr<IDXGIFactory6> fac6;
	fac.As(&fac6);

	// Grab high perf adapter
	m_adapter = select_adapter(fac6.Get());
	// .. Get logical device
	ThrowIfFailed(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(m_dev.GetAddressOf())), DET_ERR("Failed to create device"));

	// Initialize device based handle sizes
	m_hdl_sizes.init(m_dev.Get());

	// Set info queue prefs (e.g break on various severities)
	set_info_queue_prefs(m_dev.Get());
	
	// create common queues
	D3D12_COMMAND_QUEUE_DESC desc{};
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(m_dev->CreateCommandQueue(&desc, IID_PPV_ARGS(m_direct_queue.GetAddressOf())), "Failed to create a direct command queue");
	desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	ThrowIfFailed(m_dev->CreateCommandQueue(&desc, IID_PPV_ARGS(m_compute_queue.GetAddressOf())), "Failed to create a compute command queue");
	desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	ThrowIfFailed(m_dev->CreateCommandQueue(&desc, IID_PPV_ARGS(m_copy_queue.GetAddressOf())), "Failed to create a copy command queue");

	// create swapchain
	DXSwapChain::Settings sc_settings{};
	sc_settings.hwnd = settings.hwnd;
	sc_settings.max_FIF = MAX_FIF;
	sc_settings.associated_queue = m_direct_queue;
	m_sc = std::make_unique<DXSwapChain>(sc_settings, fac6.Get());
	
}

// Needed for smart ptr forward decls.
DXContext::~DXContext()
{
}

const DXContext::HandleSizes& DXContext::get_hdl_sizes()
{
	return m_hdl_sizes;
}

DXSwapChain* DXContext::get_sc()
{
	return m_sc.get();
}

ID3D12Device* DXContext::get_dev()
{
	return m_dev.Get();
}

ID3D12CommandQueue* DXContext::get_direct_queue()
{
	return m_direct_queue.Get();
}

ID3D12CommandQueue* DXContext::get_copy_queue()
{
	return m_copy_queue.Get();
}

ID3D12CommandQueue* DXContext::get_compute_queue()
{
	return m_compute_queue.Get();
}






void DXContext::HandleSizes::init(ID3D12Device* dev)
{
	cbv_srv_uav = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	sampler = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	rtv = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	dsv = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}






cptr<IDXGIAdapter> select_adapter(IDXGIFactory6* fac)
{
	UINT curr_adt_idx = 0;
	cptr<IDXGIAdapter> chosen_adt;

	DXGI_ADAPTER_DESC curr_adt_desc{};
	curr_adt_desc.DedicatedVideoMemory = (std::numeric_limits<SIZE_T>::min)();

	HRESULT hr = S_OK;
	while (true)
	{
		cptr<IDXGIAdapter> currAdapter;

		// Highest performing adapter placed at lower index
		hr = fac->EnumAdapterByGpuPreference(curr_adt_idx,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(currAdapter.GetAddressOf()));

		if (FAILED(hr))
			break;
	
		// Pick GPU with highest VRAM
		DXGI_ADAPTER_DESC d{};
		currAdapter->GetDesc(&d);
		bool higherVram = d.DedicatedVideoMemory > curr_adt_desc.DedicatedVideoMemory;
		bool deviceEligible = SUCCEEDED(D3D12CreateDevice(currAdapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr));

		if (deviceEligible && higherVram)
		{
			curr_adt_desc = d;
			chosen_adt = currAdapter;
		}
		++curr_adt_idx;
	}
	return chosen_adt;
}

void set_info_queue_prefs(ID3D12Device* dev)
{
	// Only works if Debug Layer is turned on
	// https://docs.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nn-d3d12sdklayers-id3d12infoqueue	(Scroll down)
	// https://docs.microsoft.com/en-us/windows/win32/api/dxgidebug/nn-dxgidebug-idxgiinfoqueue

	/*
		These two below actually refer to the same internal datastructure of some sort.
		You can try, because only the last one matters.
		For example, try breaking something and turning SEVERITY_ERROR break to TRUE/FALSE and see the behavior.

		Prefer to use DXGI as it is the lower level component: https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi
		Using ID3D12DebugDevice does something under the hood.. for example:
			Calling ReportLiveObjects when only ID3D12Device and ID3D12DebugDevice will state that there are (2) references of ID3D12Device.
			Presumably because ID3D12DebugDevice holds a reference to ID3D12Device inside.

		If we use DXGIDebug, we wont see that problem, since DXGI doesn't hold ID3D12 things as it is lower level! (Assuming from the diagram in above link).
	*/

	//cptr<ID3D12InfoQueue> devIQ;
	//if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(devIQ.GetAddressOf()))))
	//{
	//	// Set break points on various debug layer severities for D3D12
	//	devIQ->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
	//	devIQ->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
	//	devIQ->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
	//}

	cptr<IDXGIInfoQueue> dxgi_iq;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgi_iq.GetAddressOf()))))
	{
		// Set break points on various debug layer severities for DXGI
		dxgi_iq->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
		dxgi_iq->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);

		// We don't want to break on resources that aren't freed
		// To use the DXGIDebug and print Live Objects, we still need AT LEAST a reference to the Device
		dxgi_iq->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, FALSE);
	}
}

void append_debug_info_to_title(HWND& hwnd, bool debug_on)
{
	// get win title
	if (hwnd)
	{
		constexpr UINT max_chars = 100;
		char title[max_chars];
		auto chars = GetWindowTextA(hwnd, title, max_chars);

		std::string new_title = title;
		if (debug_on)
			new_title.append(" (Debug + Validation Layer ON)");
		else
			new_title.append(" (Debug + Validation Layer OFF)");

		SetWindowTextA(hwnd, new_title.data());
	}
}

void validate_settings(DXContext::Settings& settings)
{
	if (!settings.hwnd)
		throw std::runtime_error(DET_ERR("Headless rendering is currently not supported"));
}
