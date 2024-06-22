struct V_IN
{
    float3 Position : POSITION;
};

struct V_OUT
{
    float4 Position : SV_Position;
};

V_OUT main(V_IN input)
{
    V_OUT o;
    o.Position = float4(input.Position, 1.0f);
    return o;
}