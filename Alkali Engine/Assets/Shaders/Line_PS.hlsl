struct VS_OUT
{
    float4 Position : SV_POSITION;
    float3 Color : COLOR;
};

float4 main(VS_OUT input) : SV_TARGET
{
    return float4(input.Color, 1);
}