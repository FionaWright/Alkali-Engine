struct VS_IN
{
    float3 Position : POSITION;
    float3 Color : COLOR;
};

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float3 Color : COLOR;
};

struct Matrices
{
    matrix VP;
};

ConstantBuffer<Matrices> MatricesCB : register(b0);

VS_OUT main(VS_IN input)
{
    VS_OUT o;
    o.Position = mul(MatricesCB.VP, float4(input.Position, 1.0f)); 
    o.Color = input.Color;
    return o;
}