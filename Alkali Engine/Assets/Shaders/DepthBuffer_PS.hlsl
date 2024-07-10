struct V_OUT
{
    float4 Position : SV_Position;
};

struct DepthViewCB
{
    float2 Resolution;
    float MinValue;
    float MaxValue;
};
ConstantBuffer<DepthViewCB> DepthCB : register(b0);

Texture2D<float> DepthTexture : register(t0);
SamplerState Sampler : register(s0);

float4 main(V_OUT input) : SV_TARGET
{
    input.Position.xy /= input.Position.w;
    input.Position.xy /= DepthCB.Resolution;   
    
    float depth = DepthTexture.Sample(Sampler, input.Position.xy).r;
    depth -= DepthCB.MinValue;
    depth /= DepthCB.MaxValue - DepthCB.MinValue;
    return float4(depth, depth, depth, 1.0f);
}