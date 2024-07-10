#pragma once

#include "pch.h"

using std::wstring;

//=============Constants==============

constexpr float IM_GUI_INDENTATION = 16;

constexpr D3D12_MESSAGE_SEVERITY SEVERITIES[] = { D3D12_MESSAGE_SEVERITY_INFO };

constexpr int BACK_BUFFER_COUNT = 3;

//====================================

struct DX12Settings
{
	float NearPlane = 0.1f;
	float FarPlane = 1000.0f;

	UINT DescriptorHeapSize = 10000;

	DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT DSVFormat = DXGI_FORMAT_D32_FLOAT;
	D3D12_RESOURCE_STATES SRVFormat = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	D3D12_FILTER SamplerFilterDefault = D3D12_FILTER_ANISOTROPIC;
	D3D12_TEXTURE_ADDRESS_MODE SamplerAddressModeDefault = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	int SamplerMaxAnisotropicDefault = 16;

	UINT64 ShadowMapWidth = 2048;
	UINT64 ShadowMapHeight = 1024;
};

struct WindowSettings
{
	float ScreenWidth = 1280.0f;
	float ScreenHeight = 720.0f;

	bool UseBarrierWarps = false;	

	float FieldOfView = 45.0f;

	wstring WindowName = L"DX12 Alkali Engine Window";
};

struct MiscSettings
{
	bool AutoMipLevelsEnabled = true;
	int DefaultGlobalMipLevels = 3;
	bool CubemapMipMapsEnabled = false;

	bool CentroidBasedWorldMatricesEnabled = false;

	int IrradianceMapResolution = 512;

	bool ImGuiEnabled = true;

	enum NormalMapAcceptedChannels
	{
		ANY_CHANNELS, ONLY_2_CHANNEL, ONLY_3_CHANNEL
	} NormalMapChannels = NormalMapAcceptedChannels::ONLY_2_CHANNEL;
};

struct DynamicSettings
{
	bool VSyncEnabled = false;
	bool FullscreenEnabled = false;

	bool FrustumCullingEnabled = true;
	bool DebugLinesEnabled = true;
	bool AlwaysShowFrustumDebugLines = false;
	bool FreezeFrustum = false;

	bool HeapDebugViewEnabled = false;
	bool VisualiseDSVEnabled = false;
	bool BoundingSphereMode = false;	
	bool MipMapDebugMode = false;

	bool BatchSortingEnabled = true;
	bool AllowBinTex = true;		

	bool ShowImGuiDemoWindow = false;
};

class SettingsManager
{
public:
	const static DX12Settings ms_DX12;
	const static WindowSettings ms_Window;
	const static MiscSettings ms_Misc;
	static DynamicSettings ms_Dynamic;
};

