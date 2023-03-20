struct VisibilityPassData
{
    bool visualizeCascades;
    float4x4 invViewProj;
    uint2 screenDimension;
    uint mapBitsPerChannel;
};

cbuffer PerFrameCB : register(b0)
{
    CsmData gCsmData;
    VisibilityPassData gPass;
};

Texture2D gDepth;

float3 loadPosition(float2 UV, float depth)
{
    float2 invSize = 1.0f / gPass.screenDimension.xy;
    // Recompute position by unprojecting the depth stored in the z-buffer
    float2 ndc = 2 * UV + invSize - 1;
#ifndef FALCOR_FLIP_Y
    // NDC Y is bottom-to-top
    ndc.y = -ndc.y;
#endif
    float4 wsPos = mul(gPass.invViewProj, float4(ndc.x, ndc.y, depth, 1.f));
    return wsPos.xyz / wsPos.w;
}

float4 main(float2 texC : TEXCOORD) : SV_TARGET0
{
    //[0, 1] -> [0, 2] -> [-1, 1]
    float2 ndcXy = (texC * 2) - 1;

    float depth = gDepth[texC * gPass.screenDimension].x;
    float3 posW = loadPosition(texC, depth);

    float4 color = float4(0,0,0,0);
    color.r = calcShadowFactor(gCsmData, depth, posW, ndcXy);

    if(gPass.visualizeCascades)
    {
        color.gba = getBlendedCascadeColor(gCsmData, depth);
    }

    return color;
}
