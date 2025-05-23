#define DRAW_SPACE space0
#define FRAME_SPACE space1

#define SRV_SPACE space0
#define SRV_DYNAMIC_SPACE space1

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
ConstantBuffer<DepthViewCB> DepthCB : register(b0, DRAW_SPACE);

Texture2D<float> DepthTexture : register(t0, SRV_DYNAMIC_SPACE);
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