#pragma once
#include <windows.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <d3d12.h>
#include <functional>
#include <unordered_map>

class GUIContext
{
public:
	GUIContext(
		HWND hwnd, ID3D12Device* dev, uint32_t max_fif,
		ID3D12DescriptorHeap* cbv_srv_dheap,					// Supply a Shader-Visible descriptor heap used when rendering UI.
		D3D12_CPU_DESCRIPTOR_HANDLE reserved_cpu_handle,		// ImGUI will use a single descriptor in this heap at the handle location
		D3D12_GPU_DESCRIPTOR_HANDLE reserved_gpu_handle,		// Assumed that CPU/GPU desc handle are from the supplied descriptor heap
		DXGI_FORMAT rtv_format = DXGI_FORMAT_R8G8B8A8_UNORM);	
	~GUIContext();

	void frame_begin();
	void render(ID3D12GraphicsCommandList* cmdl);
	void frame_end();
	
	// Add/remove persistent UI
	void add_persistent_ui(const std::string& name, const std::function<void()> callback);
	void remove_persistent_ui(const std::string& name);

	bool win_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


private:
	std::unordered_map<std::string, std::function<void()>> m_persistent_ui_callbacks;

};

