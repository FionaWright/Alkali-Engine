#pragma once

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

constexpr wchar_t WINDOW_CLASS_NAME[] = L"DX12 Alkali Engine Window";

constexpr UINT BACK_BUFFER_COUNT = 3;

constexpr bool USE_B_WARP = false;

// GPU Debug
constexpr D3D12_MESSAGE_SEVERITY SEVERITIES[] =
{
    D3D12_MESSAGE_SEVERITY_INFO
};