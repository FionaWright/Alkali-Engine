#define PI 3.141592654f
#define SAMPLE_COUNT 1024u

TextureCube<float4> SrcTexture : register(t0);
RWTexture2D<float4> DstTexture : register(u0);
SamplerState BilinearClamp : register(s0);

cbuffer CB : register(b0)
{
    float2 TexelSize;
    float Roughness;
    uint Face;
}

float RadicalInverse_VdC(uint bits);
float2 RNG_Hammersley(uint i, uint N);
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness);
float3 GetNormalFromTexCoord(uint face, uint2 texCoord, uint2 faceReso);

[numthreads(8, 8, 1)]
void GenerateMipMaps(uint3 DTid : SV_DispatchThreadID)
{    
    uint2 faceReso;
    DstTexture.GetDimensions(faceReso.x, faceReso.y);
    
    float3 N = GetNormalFromTexCoord(Face, DTid.xy, faceReso);
    float3 R = N;
    float3 V = R;
    
    //DstTexture[DTid.xy] = float4(N, 1);
    //return;

    float totalWeight = 0.0;
    float3 prefilteredColor = float3(0.0, 0.0, 0.0);

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = RNG_Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, Roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            prefilteredColor += SrcTexture.Sample(BilinearClamp, L).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / totalWeight;
    DstTexture[DTid.xy] = float4(prefilteredColor, 1.0);
}
    
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Low discrepency psuedo random number generator
float2 RNG_Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
	
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	
    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
	
    // from tangent-space vector to world-space sample vector
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
	
    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

float3 GetNormalFromTexCoord(uint face, uint2 texCoord, uint2 faceReso)
{
    float2 uv = (float2(texCoord) / float2(faceReso - 1)); 
    uv = uv * 2.0f - 1.0f; // Range of [-1, 1]

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
        default:
            return 0;
    }

    return normalize(normal);
}