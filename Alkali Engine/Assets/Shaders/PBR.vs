struct V_IN
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
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
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL;    
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
};

V_OUT main(V_IN input)
{
    V_OUT o;

    float4 pos = float4(input.Position, 1.0f);
    o.Position = mul(mul(MatricesCB.VP, MatricesCB.M), pos);
    o.UV = input.UV;
    //o.UV = frac(input.UV);

    //float2 positive = input.UV > 0;
    //o.UV = (input.UV - floor(input.UV)) * positive + (1 - positive) * (input.UV - ceil(input.UV));

    o.Normal = normalize(mul((float3x3)MatricesCB.InverseTransposeM, input.Normal));
    o.Tangent = normalize(mul((float3x3)MatricesCB.InverseTransposeM, input.Tangent));
    o.Binormal = normalize(mul((float3x3)MatricesCB.InverseTransposeM, input.Binormal));

    return o;
}