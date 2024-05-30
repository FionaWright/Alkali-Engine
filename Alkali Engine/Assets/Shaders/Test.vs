struct V_IN
{
    float3 Position : POSITION;
    float2 Texture : TEXCOORD0;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
};

struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct V_OUT
{
    float3 Normal : NORMAL;
    float4 Position : SV_Position;
};

V_OUT main(V_IN input)
{
    V_OUT o;

    o.Position = mul(ModelViewProjectionCB.MVP, float4(input.Position, 1.0f));
    o.Normal = input.Normal;

    return o;
}