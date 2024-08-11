struct V_OUT
{
    float4 Position : SV_Position;
    float3 ViewDirection : TEXCOORD0;
};

TextureCube g_cubemap : register(t0);
SamplerState g_sampler : register(s0);

float4 main(V_OUT input) : SV_Target
{
    float3 col = g_cubemap.SampleLevel(g_sampler, input.ViewDirection, 0).rgb;
    return float4(col, 1);
}