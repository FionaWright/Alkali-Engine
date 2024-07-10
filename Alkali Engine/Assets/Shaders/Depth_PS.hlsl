struct V_OUT
{
    float4 Position : SV_Position;
};

float4 main(V_OUT input) : SV_Target
{
    float near = 0.1f;
    float far = 1000.0f;
    
    input.Position.xyz /= input.Position.w;
    return (input.Position.z - near) / (far - near);
}