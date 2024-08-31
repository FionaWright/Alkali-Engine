#define DRAW_SPACE space0
#define FRAME_SPACE space1

struct V_OUT
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
};

struct CostValue
{
    float Cost;
    float3 p;
};
ConstantBuffer<CostValue> CostCB : register(b1, DRAW_SPACE);

float4 main(V_OUT input) : SV_Target
{
    return float4(CostCB.Cost, 1 - CostCB.Cost, 0, 1);
}