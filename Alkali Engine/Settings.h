#pragma once

#define SCREEN_WIDTH 1280.0f
#define SCREEN_HEIGHT 720.0f
constexpr bool USING_IM_GUI = true;

constexpr float IM_GUI_INDENTATION = 16;

constexpr float ASPECT_RATIO = SCREEN_WIDTH / SCREEN_HEIGHT;

constexpr wchar_t WINDOW_CLASS_NAME[] = L"DX12 Alkali Engine Window";

// DX12
constexpr UINT BACK_BUFFER_COUNT = 3;
constexpr bool USE_B_WARP = false;
constexpr DXGI_FORMAT SWAP_CHAIN_DXGI_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 300;

// GPU Debug
constexpr D3D12_MESSAGE_SEVERITY SEVERITIES[] =
{
    D3D12_MESSAGE_SEVERITY_INFO
};

// MATH
inline constexpr double DEG_TO_RAD = 0.01745329252;
inline constexpr double RAD_TO_DEG = 57.295779513;
inline constexpr double PI = 3.14159265359;