struct V_IN
{
    float3 Position : POSITION;
    float3 Color : COLOR;
};

struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct V_OUT
{
    float4 Color : COLOR;
    float4 Position : SV_Position;
};

V_OUT main(V_IN input)
{
    V_OUT o;

    o.Position = mul(ModelViewProjectionCB.MVP, float4(input.Position, 1.0f));
    o.Color = float4(input.Color, 1.0f);

    return o;
}