Texture2D<float4> SrcTexture : register(t0);
RWTexture2D<float4> DstTexture : register(u0);
SamplerState BilinearClamp : register(s0);

cbuffer CB : register(b0)
{
    float2 TexelSize; // 1.0 / destination dimension
}

// NEEDS TO BE CHANGED TO WORK WITH SRGB
[numthreads(8, 8, 1)]
void GenerateMipMaps(uint3 DTid : SV_DispatchThreadID)
{
	//DTid is the thread ID * the values from numthreads above and in this case correspond to the pixels location in number of pixels.
	//As a result texcoords (in 0-1 range) will point at the center between the 4 pixels used for the mipmap.
    float2 texcoordsNW = TexelSize * (DTid.xy);
    float2 texcoordsNE = TexelSize * (DTid.xy + float2(1, 0));
    float2 texcoordsSW = TexelSize * (DTid.xy + float2(0, 1));
    float2 texcoordsSE = TexelSize * (DTid.xy + float2(1, 1));

    float4 colorNW = SrcTexture.SampleLevel(BilinearClamp, texcoordsNW, 0);
    float4 colorNE = SrcTexture.SampleLevel(BilinearClamp, texcoordsNE, 0);
    float4 colorSW = SrcTexture.SampleLevel(BilinearClamp, texcoordsSW, 0);
    float4 colorSE = SrcTexture.SampleLevel(BilinearClamp, texcoordsSE, 0);
    
    colorNW.rgb = pow(colorNW.rgb, 2.2);
    colorNE.rgb = pow(colorNE.rgb, 2.2);
    colorSW.rgb = pow(colorSW.rgb, 2.2);
    colorSE.rgb = pow(colorSE.rgb, 2.2);
    
    float4 averagedColor = (colorNW + colorNE + colorSW + colorSE) / 4.0f;
    averagedColor.rgb = pow(averagedColor.rgb, 1.0f / 2.2f);

	//Write the final color into the destination texture.
    DstTexture[DTid.xy] = averagedColor;
}