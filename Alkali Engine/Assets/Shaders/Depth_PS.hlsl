struct V_OUT
{
    float4 Position : SV_Position;
};

Texture2D<float> DepthTexture : register(t0);
SamplerState Sampler : register(s0);

float4 main(V_OUT input) : SV_TARGET
{
    input.Position.xy /= input.Position.w;
    input.Position.xy /= float2(1280, 720);
    
    float depth = DepthTexture.Sample(Sampler, input.Position.xy).r;
    depth -= 0.98f;
    depth *= 10;
    return float4(depth, depth, depth, 1.0f);
}