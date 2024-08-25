struct V_OUT
{
    float4 Position : SV_Position;
};

float main(V_OUT input) : SV_Target
{
    float depth = input.Position.z / input.Position.w;
    return depth;
}