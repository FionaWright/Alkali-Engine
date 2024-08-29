#define DRAW_SPACE space0
#define FRAME_SPACE space1

struct V_IN
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
};

struct MatricesMV_V
{
    matrix M;
    matrix V;
};
ConstantBuffer<MatricesMV_V> MatricesCB : register(b0, DRAW_SPACE);

struct V_OUT
{
    float4 Position : POS;
    float2 UV : TEXCOORD0;
};

V_OUT main(V_IN input)
{
    V_OUT o;

    float4 pos = float4(input.Position, 1.0f);
    float4 worldPos = mul(MatricesCB.M, pos);
    o.Position = mul(MatricesCB.V, worldPos);
    
    o.UV = input.UV;

    return o;
}