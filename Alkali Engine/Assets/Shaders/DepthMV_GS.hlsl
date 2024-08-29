#define DRAW_SPACE space0
#define FRAME_SPACE space1

struct V_OUT
{
    float4 Position : POS;
    float2 UV : TEXCOORD0;
};

struct G_OUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
    uint ViewportIndex : SV_ViewportArrayIndex;
};

struct MatricesMV_G
{
    matrix P[4];
};
ConstantBuffer<MatricesMV_G> MatricesCB : register(b0, FRAME_SPACE);

struct CullFlags
{
    uint Flags;
};
ConstantBuffer<CullFlags> CullFlagsCB : register(b1, DRAW_SPACE);

[maxvertexcount(4 * 3)] // 4 Viewports, 3 Vertices per Triangle?
void main(triangle V_OUT input[3], inout TriangleStream<G_OUT> triStream)
{
    for (uint vp = 0; vp < 4; vp++)
    {
        if (!(CullFlagsCB.Flags & 1 << vp))
            continue;
        
        G_OUT o[3];
        
        for (uint i = 0; i < 3; i++)
        {
            o[i].Position = mul(MatricesCB.P[vp], input[i].Position);
            o[i].UV = input[i].UV;
            o[i].ViewportIndex = vp;
        }
        
        triStream.Append(o[0]);
        triStream.Append(o[1]);
        triStream.Append(o[2]);
        triStream.RestartStrip();
    }
}