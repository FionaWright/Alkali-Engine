struct V_IN
{
    float3 Position : POSITION;
};

struct Matrices
{
    matrix M;
    matrix InverseTransposeM;
    matrix VP;
};
ConstantBuffer<Matrices> MatricesCB : register(b0);

struct V_OUT
{
    float4 Position : SV_Position;
};

V_OUT main(V_IN input)
{
    V_OUT o;

    float4 pos = float4(input.Position, 1.0f);
    float4 worldPos = mul(MatricesCB.M, pos);
    o.Position = mul(MatricesCB.VP, worldPos);   

    return o;
}