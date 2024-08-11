Texture2DArray<float4> SrcTexture : register(t0);
RWTexture2DArray<float4> DstTexture : register(u0);
SamplerState BilinearClamp : register(s0);

cbuffer CB : register(b0)
{
    float2 TexelSize;
}

float3 GetNormalFromCubemapCoordinates(uint face, uint2 texCoord, uint2 dimensions);

[numthreads(8, 8, 1)]
void GenerateMipMaps(uint3 DTid : SV_DispatchThreadID)
{
    //DstTexture[DTid] = float4(DTid, 1);
    DstTexture[DTid] = float4(1, 0, 0, 1);
    return;
    
    uint2 dimensions;
    int elements;
    DstTexture.GetDimensions(dimensions.x, dimensions.y, elements);
    
    float2 texcoordsC = TexelSize * (DTid.xy);
    float2 texcoordsN = TexelSize * (DTid.xy + float2(0, 1));
    float2 texcoordsE = TexelSize * (DTid.xy + float2(1, 0));
    float2 texcoordsS = TexelSize * (DTid.xy + float2(0, -1));
    float2 texcoordsW = TexelSize * (DTid.xy + float2(-1, 0));
    
    float3 dirC = GetNormalFromCubemapCoordinates(DTid.z, texcoordsC, dimensions);
    float3 dirN = GetNormalFromCubemapCoordinates(DTid.z, texcoordsN, dimensions);
    float3 dirE = GetNormalFromCubemapCoordinates(DTid.z, texcoordsE, dimensions);
    float3 dirS = GetNormalFromCubemapCoordinates(DTid.z, texcoordsS, dimensions);
    float3 dirW = GetNormalFromCubemapCoordinates(DTid.z, texcoordsW, dimensions);

    float4 colorC = SrcTexture.Sample(BilinearClamp, dirC);
    float4 colorN = SrcTexture.Sample(BilinearClamp, dirN);
    float4 colorE = SrcTexture.Sample(BilinearClamp, dirE);
    float4 colorS = SrcTexture.Sample(BilinearClamp, dirS);
    float4 colorW = SrcTexture.Sample(BilinearClamp, dirW);
    
    colorN.rgb = pow(colorN.rgb, 2.2);
    colorE.rgb = pow(colorE.rgb, 2.2);
    colorS.rgb = pow(colorS.rgb, 2.2);
    colorW.rgb = pow(colorW.rgb, 2.2);
    colorC.rgb = pow(colorC.rgb, 2.2);
    
    float4 averagedColor = (colorN + colorE + colorS + colorW + colorC) / 5.0f;
    averagedColor.rgb = pow(averagedColor.rgb, 1.0f / 2.2f);

    DstTexture[DTid] = averagedColor;
}

float3 GetNormalFromCubemapCoordinates(uint face, uint2 texCoord, uint2 dimensions)
{
    float2 uv = (float2(texCoord) / float2(dimensions - 1)) * 2.0 - 1.0;

    float3 normal;
    switch (face)
    {
        case 0: // +X
            normal = float3(1.0, -uv.y, -uv.x);
            break;
        case 1: // -X
            normal = float3(-1.0, -uv.y, uv.x);
            break;
        case 2: // +Y
            normal = float3(uv.x, 1.0, uv.y);
            break;
        case 3: // -Y
            normal = float3(uv.x, -1.0, -uv.y);
            break;
        case 4: // +Z
            normal = float3(uv.x, -uv.y, 1.0);
            break;
        case 5: // -Z
            normal = float3(-uv.x, -uv.y, -1.0);
            break;
    }

    return normalize(normal);
}