#include "pch.h"
#include "GUIContext.h"

GUIContext::GUIContext(HWND hwnd, ID3D12Device* dev, uint32_t max_fif,
	ID3D12DescriptorHeap* cbv_srv_dheap, D3D12_CPU_DESCRIPTOR_HANDLE reserved_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE reserved_gpu_handle,
	DXGI_FORMAT rtv_format) 
{
	// Setup ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io.FontGlobalScale = 1.f;
    //io.ConfigDockingWithShift = true;

    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(dev, max_fif,
		rtv_format, cbv_srv_dheap,
		reserved_cpu_handle,
		reserved_gpu_handle);

}

GUIContext::~GUIContext()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void GUIContext::frame_begin()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// make dockspace over the whole viewport
	// this has to be called prior to all other ImGUI calls
	ImGuiDockNodeFlags flags = ImGuiDockNodeFlags_PassthruCentralNode;      // pass through the otherwise background cleared window
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), flags);
}

void GUIContext::render(ID3D12GraphicsCommandList* cmdl)
{
	// Trigger ImGUI code which are submitted by app for persistent display
	for (const auto& [name, func] : m_persistent_ui_callbacks)
		func();

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdl);
}

void GUIContext::frame_end()
{
	ImGuiIO& io = ImGui::GetIO();

	// Update and Render additional Platform Windows
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void GUIContext::add_persistent_ui(const std::string& name, const std::function<void()> callback)
{
	auto& cb = m_persistent_ui_callbacks[name];
	cb = callback;
}

void GUIContext::remove_persistent_ui(const std::string& name)
{
	m_persistent_ui_callbacks.erase(name);
}
