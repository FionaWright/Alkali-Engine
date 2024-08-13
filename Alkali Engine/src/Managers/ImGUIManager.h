#pragma once

#include "pch.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

class ImGUIManager
{
public:
	static void Init(HWND hwnd, ID3D12Device2* device, int framesInFlight);

	static void Begin();
	static void Render(ID3D12GraphicsCommandList* cmdList);

	static bool MouseHoveringImGui();

	static void Shutdown();	

private:
	static ComPtr<ID3D12DescriptorHeap> ms_descHeapSRV;
	static bool ms_currentlyRendering;
	static ImVec2 ms_windowSize;
};