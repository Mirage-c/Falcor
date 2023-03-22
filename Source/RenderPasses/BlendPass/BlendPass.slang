
cbuffer PerFrameCB
{
    uint2 screenDimension;
};

Texture2D srcTex1;
Texture2D srcTex2;

float4 main(float2 texC: TEXCOORD) : SV_TARGET0
{
    float2 uv = texC * screenDimension;
    float3 color1 = srcTex1[uv].xyz;
    float3 color2 = srcTex2[uv].xyz;
    return float4(color1 + color2, 1.f);
}