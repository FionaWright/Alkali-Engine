#pragma once

#define SCREEN_WIDTH 1280.0f
#define SCREEN_HEIGHT 720.0f
constexpr bool USING_IM_GUI = true;
constexpr bool PERMA_FRUSTUM_DEBUG_LINES = false;
constexpr bool FRUSTUM_CULLING_ENABLED = true;

constexpr float IM_GUI_INDENTATION = 16;

constexpr float ASPECT_RATIO = SCREEN_WIDTH / SCREEN_HEIGHT;

constexpr wchar_t WINDOW_CLASS_NAME[] = L"DX12 Alkali Engine Window";

// DX12
constexpr UINT BACK_BUFFER_COUNT = 3;
constexpr bool USE_BARRIER_WARP = false;

constexpr bool USE_SRGB = false; // Leave off, breaks stuff

constexpr int GLOBAL_MIP_LEVELS = 5;
constexpr bool AUTO_MIP_LEVELS = true;
constexpr bool MIP_MAP_DEBUG_MODE = false;

constexpr bool CENTROID_BASED_WORLD_MATRIX_ENABLED = false;

constexpr DXGI_FORMAT SWAP_CHAIN_DXGI_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

constexpr DXGI_FORMAT TEXTURE_DIFFUSE_DXGI_FORMAT = USE_SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT TEXTURE_NORMAL_MAP_DXGI_FORMAT = DXGI_FORMAT_R8G8_UNORM;

constexpr DXGI_FORMAT RTV_FORMAT = USE_SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT DSV_FORMAT = DXGI_FORMAT_D32_FLOAT;

constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 1000;

constexpr XMFLOAT3 XMFLOAT3_ZERO = XMFLOAT3(0, 0, 0);
constexpr XMFLOAT3 XMFLOAT3_ONE = XMFLOAT3(1, 1, 1);

// GPU Debug
constexpr D3D12_MESSAGE_SEVERITY SEVERITIES[] =
{
    D3D12_MESSAGE_SEVERITY_INFO
};

// MATH
inline constexpr double DEG_TO_RAD = 0.01745329252;
inline constexpr double RAD_TO_DEG = 57.295779513;
inline constexpr double PI = 3.14159265359;