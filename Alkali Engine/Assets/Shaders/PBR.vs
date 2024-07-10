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
    float3 ShadowMapCoords : TEXCOORD2;
};

struct Matrices
{
    matrix M;
    matrix InverseTransposeM;
    matrix VP;
};
ConstantBuffer<Matrices> MatricesCB : register(b0);

struct Camera
{
    float3 CameraPosition;
    float p;
};
ConstantBuffer<Camera> CameraCB : register(b2);

struct ShadowMap
{
    matrix ShadowMatrix;
};
ConstantBuffer<ShadowMap> ShadowCB : register(b4);

V_OUT main(V_IN input)
{
    V_OUT o;

    float4 pos = float4(input.Position, 1.0f);
    float4 worldPos = mul(MatricesCB.M, pos);
    o.Position = mul(MatricesCB.VP, worldPos);
    o.UV = input.UV;

    o.Normal = normalize(mul((float3x3)MatricesCB.InverseTransposeM, input.Normal));
    o.Tangent = normalize(mul((float3x3)MatricesCB.InverseTransposeM, input.Tangent));
    o.Binormal = normalize(mul((float3x3)MatricesCB.InverseTransposeM, input.Binormal));

    o.ViewDirection = CameraCB.CameraPosition.xyz - worldPos.xyz;
    o.ViewDirection = normalize(o.ViewDirection);

    float4 shadowPos = mul(ShadowCB.ShadowMatrix, worldPos);
    o.ShadowMapCoords = shadowPos.xyz / shadowPos.w;
    o.ShadowMapCoords.xy = o.ShadowMapCoords.xy * 0.5f + 0.5f;

    return o;
}