struct V_IN
{
    float3 Position : POSITION;
};

struct V_OUT
{
    float4 Position : SV_Position;
    float3 ViewDirection : TEXCOORD0;
};

struct Matrices
{
    matrix M;
    matrix InverseTransposeM;
    matrix V;
    matrix P;
};
ConstantBuffer<Matrices> MatricesCB : register(b0);

V_OUT main(V_IN input)
{
    V_OUT o;

    float4 pos = float4(input.Position, 1.0f);
    
    float4 worldPos = mul(MatricesCB.M, pos);
    o.Position = mul(mul(MatricesCB.P, MatricesCB.V), worldPos);
    
    o.Position.z = o.Position.w;

    o.ViewDirection = normalize(input.Position.xyz);

    return o;
}