#pragma once

#include "pch.h"

using std::wstring;

//=============Constants==============

constexpr float IM_GUI_INDENTATION = 16;

constexpr D3D12_MESSAGE_SEVERITY SEVERITIES[] = { D3D12_MESSAGE_SEVERITY_INFO };

constexpr int BACK_BUFFER_COUNT = 3;

constexpr int MAX_SHADOW_MAP_CASCADES = 4;

//====================================

struct DX12Settings
{
	bool DebugGlobalDescriptorHeapEnabled = true;
	UINT DescriptorHeapSize = 60000;

	bool DebugLoadSingleSceneOnly = false;
	bool DebugWhiteTextureOnly = false;
	bool DebugCubeModelOnly = false;

	DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT DSVFormat = DXGI_FORMAT_D32_FLOAT;
	D3D12_RESOURCE_STATES SRVFormat = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	D3D12_STATIC_SAMPLER_DESC DefaultSamplerDesc = {
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		0, // MipLODBias
		16, // Max Anisotropic
		D3D12_COMPARISON_FUNC_NEVER,
		D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		0.0f, // MinLOD
		D3D12_FLOAT32_MAX, // MaxLOD
		0, // Shader Register
		0, // Register Space
		D3D12_SHADER_VISIBILITY_PIXEL
	};

	bool MSAAEnabled = false; // Unsupported for flip models

	bool EnableValidationLayerOnReleaseMode = false;
	bool EnableAllValidationLayerMessages = false;
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
	int DefaultGlobalMipLevels = 1;
	bool CubemapMipMapsEnabled = true;
	bool RequireAlphaTextureForDoubleSided = true;
	bool BistroLowQualityTexDiffuse = false;
	bool BistroLowQualityTexNormal = true;

	bool CentroidBasedWorldMatricesEnabled = false;
	XMFLOAT3 MaxCameraPosition = XMFLOAT3(10000, 10000, 10000);

	int IrradianceMapResolution = 512;

	bool ImGuiEnabled = true;
	int HotReloadTimeSlice = 60;

	int ShadowMapResoWidth = 2048;
	int ShadowMapResoHeight = 2048;
	bool ShadowCullFront = false;
	bool ShadowHDFormat = false;	

	float DebugLinesFarPlane = 2000.0f;

	enum NormalMapAcceptedChannels
	{
		ANY_CHANNELS, ONLY_2_CHANNEL, ONLY_3_CHANNEL
	} NormalMapChannels = NormalMapAcceptedChannels::ONLY_2_CHANNEL;
};

struct ShadowDynamicSettings
{
	int CascadeCount = 4;

	bool Enabled = true;
	bool UpdatingBounds = true;
	bool Rendering = true;	

	bool BoundToScene = false;
	bool UseBoundingSpheres = true;
	bool CullAgainstBounds = true;

	bool ShowDebugBounds = false;

	int PCFSampleCount = 16;
	float BoundsBias = 5.0f;
	int TimeSlice = 10;

	bool AutoNearFarPercent = true;
	float NearPercents[MAX_SHADOW_MAP_CASCADES];
	float FarPercents[MAX_SHADOW_MAP_CASCADES];	
};

struct DynamicSettings
{
	float NearPlane = 0.1f;
	float FarPlane = 200.0f;
	bool VSyncEnabled = false;
	bool FullscreenEnabled = false;
	bool ForceSyncCPUGPU = false;
	float UpdateTimeScale = 1.0f;

	bool WireframeMode = false;
	bool CullBackFaceEnabled = true;

	bool FrustumCullingEnabled = true;
	bool DebugLinesEnabled = true;
	bool AlwaysShowFrustumDebugLines = false;
	bool FreezeFrustum = false;

	bool HeapDebugViewEnabled = false;
	bool VisualiseDSVEnabled = false;
	bool BoundingSphereMode = false;	
	bool MipMapDebugMode = false;
	bool VisualiseShadowMap = false;

	ShadowDynamicSettings Shadow;

	bool BatchSortingEnabled = true;
	bool AllowBinTex = true;		

	bool ShowImGuiDemoWindow = false;

	bool QuickDebug = false; // Useful for debugging code quickly without making new GUI 
};

class SettingsManager
{
public:
	const static DX12Settings ms_DX12;
	const static WindowSettings ms_Window;
	const static MiscSettings ms_Misc;
	static DynamicSettings ms_Dynamic;
};

