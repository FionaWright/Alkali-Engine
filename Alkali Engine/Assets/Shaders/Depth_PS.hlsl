#define DRAW_SPACE space0
#define FRAME_SPACE space1

#define SRV_SPACE space0
#define SRV_DYNAMIC_SPACE space1

#define ALPHA_TEST 0.5f

struct V_OUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

Texture2D<float4> g_texture : register(t0, SRV_SPACE);
SamplerState g_sampler : register(s0);

void main(V_OUT input)
{
    float alpha = g_texture.Sample(g_sampler, input.UV).a > ALPHA_TEST;
    clip(alpha - 0.5f);
}