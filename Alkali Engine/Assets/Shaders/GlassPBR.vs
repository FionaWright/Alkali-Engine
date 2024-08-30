#define CASCADES 4

#define DRAW_SPACE space0
#define FRAME_SPACE space1

struct V_IN
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
};

struct V_OUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL;    
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;

    float3 ViewDirection : TEXCOORD1;    
    float ViewDepth : TEXCOORD2;

#ifdef SHADOW_ENABLED
    float4 ShadowMapCoords[CASCADES] : TEXCOORD3;
#endif
};

struct Matrices
{
    matrix M;
    matrix InverseTransposeM;
    matrix V;
    matrix P;
};
ConstantBuffer<Matrices> MatricesCB : register(b0, DRAW_SPACE);

struct Camera
{
    float3 CameraPosition;
    float p;
};
ConstantBuffer<Camera> CameraCB : register(b0, FRAME_SPACE);

struct ShadowMap
{
    float NormalBias;
    float CascadeCount;
    float2 p;

    matrix ShadowMatrix[CASCADES];
};
ConstantBuffer<ShadowMap> ShadowCB : register(b2, FRAME_SPACE);

V_OUT main(V_IN input)
{
    V_OUT o;

    float4 pos = float4(input.Position, 1.0f);
    float4 worldPos = mul(MatricesCB.M, pos);
    o.Position = mul(mul(MatricesCB.P, MatricesCB.V), worldPos);
    o.UV = input.UV;

    o.Normal = normalize(mul((float3x3)MatricesCB.InverseTransposeM, input.Normal));
    o.Tangent = normalize(mul((float3x3)MatricesCB.InverseTransposeM, input.Tangent));
    o.Binormal = normalize(mul((float3x3)MatricesCB.InverseTransposeM, input.Binormal));

    o.ViewDirection = CameraCB.CameraPosition.xyz - worldPos.xyz;
    o.ViewDirection = normalize(o.ViewDirection);

#ifdef SHADOW_ENABLED
    for (int i = 0; i < ShadowCB.CascadeCount; i++)
    {
        float4 biasedPos = float4(worldPos.xyz + o.Normal * ShadowCB.NormalBias, worldPos.w);
        float4 shadowPos = mul(ShadowCB.ShadowMatrix[i], biasedPos);
        o.ShadowMapCoords[i].xyz = shadowPos.xyz / shadowPos.w;
        o.ShadowMapCoords[i].xy = o.ShadowMapCoords[i].xy * 0.5f + 0.5f;
    }
#endif

    o.ViewDepth = mul(MatricesCB.V, worldPos).z;

    return o;
}