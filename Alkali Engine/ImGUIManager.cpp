#include "pch.h"
#include "ImGUIManager.h"
#include "ResourceManager.h"

ComPtr<ID3D12DescriptorHeap> ImGUIManager::m_descHeapSRV;

void ImGUIManager::Init(HWND hwnd, ComPtr<ID3D12Device2> device, int framesInFlight, DXGI_FORMAT format)
{
	if (!USING_IM_GUI)
		return;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	int backBufferCount = 1;
	m_descHeapSRV = ResourceManager::CreateDescriptorHeap(backBufferCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device.Get(), framesInFlight, format, m_descHeapSRV.Get(), m_descHeapSRV->GetCPUDescriptorHandleForHeapStart(), m_descHeapSRV->GetGPUDescriptorHandleForHeapStart());
}

void ImGUIManager::Begin()
{
	if (!USING_IM_GUI)
		return;

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow(); // Show demo window! :)
}

void ImGUIManager::Render(ComPtr<ID3D12GraphicsCommandList> commandList)
{
	if (!USING_IM_GUI)
		return;

	ImGui::Render();
	ID3D12DescriptorHeap* pDescHeap = m_descHeapSRV.Get();
	commandList->SetDescriptorHeaps(1, &pDescHeap);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

	// Execute command list
}

void ImGUIManager::Shutdown()
{
	if (!USING_IM_GUI)
		return;

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_descHeapSRV.Reset();
}
