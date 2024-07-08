struct V_IN
{
    float3 Position : POSITION;
    float2 Texture : TEXCOORD0;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
};

struct Matrices
{
    matrix M;
    matrix InverseTransposeM;
    matrix VP;
};
ConstantBuffer<Matrices> MatricesCB : register(b0);

struct V_OUT
{
    float3 Normal : NORMAL;
    float4 Position : SV_Position;
};

V_OUT main(V_IN input)
{
    V_OUT o;

    float4 pos = float4(input.Position, 1.0f);
    float4 worldPos = mul(MatricesCB.M, pos);
    o.Position = mul(MatricesCB.VP, worldPos);
    
    o.Normal = normalize(mul((float3x3) MatricesCB.InverseTransposeM, input.Normal));

    return o;
}