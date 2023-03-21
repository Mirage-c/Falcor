import Utils.Sampling.TinyUniformSampleGenerator;

cbuffer PerFrameCB
{
    uint2 screenDimension;
};

Texture2D gNormalTex;
Texture2D gWorldPosTex;

Texture2D rNormalTex;
Texture2D rFluxTex;
Texture2D rWorldPosTex;

Texture2D samplesTex;

float4 main(float2 texC: TEXCOORD) : SV_TARGET0
{
    return float4(0.5f, 0.5f, 1.0f, 1.0f);
}