#include "pch.h"
#include "ImGUIManager.h"
#include "ResourceManager.h"
#include "InputManager.h"

ComPtr<ID3D12DescriptorHeap> ImGUIManager::ms_descHeapSRV;
bool ImGUIManager::ms_currentlyRendering;
ImVec2 ImGUIManager::ms_windowSize;
bool ImGUIManager::ms_initialised;

void ImGUIManager::Init(HWND hwnd, ID3D12Device2* device, int framesInFlight)
{
	if (!SettingsManager::ms_Misc.ImGuiEnabled)
		return;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	int backBufferCount = 1;
	ms_descHeapSRV = ResourceManager::CreateDescriptorHeap(backBufferCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

	auto rtvFormat = SettingsManager::ms_DX12.RTVFormat;

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device, framesInFlight, rtvFormat, ms_descHeapSRV.Get(), ms_descHeapSRV->GetCPUDescriptorHandleForHeapStart(), ms_descHeapSRV->GetGPUDescriptorHandleForHeapStart());

	ms_initialised = true;
}

void ImGUIManager::Begin()
{
	if (!SettingsManager::ms_Misc.ImGuiEnabled || ms_currentlyRendering)
		return;	

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();	

	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(500, 800), ImGuiCond_FirstUseEver);	

	ImGui::Begin("Alkali Engine GUI", nullptr, ImGuiWindowFlags_None);
	ms_windowSize = ImGui::GetWindowSize();

	ms_currentlyRendering = true;
}

void ImGUIManager::Render(ID3D12GraphicsCommandList* cmdList)
{
	if (!SettingsManager::ms_Misc.ImGuiEnabled || !ms_currentlyRendering)
		return;

	ImGui::End();
	ImGui::Render();

	ID3D12DescriptorHeap* pDescHeap = ms_descHeapSRV.Get();
	cmdList->SetDescriptorHeaps(1, &pDescHeap);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

	ms_currentlyRendering = false;
}

bool ImGUIManager::MouseHoveringImGui()
{
	XMFLOAT2 mousePos = InputManager::GetMousePos();
	return mousePos.x < ms_windowSize.x && mousePos.y < ms_windowSize.y;
}

void ImGUIManager::Shutdown()
{
	if (!SettingsManager::ms_Misc.ImGuiEnabled || !ms_initialised)
		return;

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	ms_descHeapSRV.Reset();

	ms_currentlyRendering = false;
}
