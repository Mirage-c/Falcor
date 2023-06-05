// FullScreenPass to spread SSR indirect radiance from one pixel to its adjacent ones

import Utils.Math.MatrixUtils;
#ifndef SPREAD_RADIUS
#define SPREAD_RADIUS 15
#endif
#ifndef SAMPLE_NUM
#define SAMPLE_NUM 0
#endif

cbuffer PerFrameCB
{
    uint2 screenDimension;
    uint frameCnt;
}

Texture2D gColorTexSSR; // SSR indirect
Texture2D gNormalTex;


float4 main(float2 texC: TEXCOORD) : SV_TARGET0
{
    // the texC pixel reads its adjacent pixel
    int2 screenPos = texC * screenDimension;
    float3 normal = gNormalTex[screenPos].xyz * 2.0f - 1.0f;
    int spreadNum = 1;
    float3 spreadedIndirect = gColorTexSSR[screenPos].xyz;

    float s = InitRand(texC + frameCnt / 1128);
    for (int i = 0; i < SAMPLE_NUM; i++) {
        s += float(i);
        float2 rand = Rand2(s);
        float r = SPREAD_RADIUS * rand.x;
        float phi = rand.y * 6.283185307; // 2pi
        float2 relativePos = float2(r * cos(phi), r * sin(phi));
        int2 adjacentPos = screenPos + relativePos;
        if (adjacentPos.x > screenDimension[0] || adjacentPos.x < 0 || adjacentPos.y > screenDimension[1] || adjacentPos.y < 0)
            continue;
        float3 adjacentNorm = gNormalTex[adjacentPos].xyz * 2.0f - 1.0f;
        if (all(normal == adjacentNorm)) {
            // get SSR color and spread
            float4 ssrRadiance = gColorTexSSR[adjacentPos];
            spreadNum++;
            spreadedIndirect += ssrRadiance.xyz;
        }
    }

    /*for (int i = -SPREAD_RADIUS; i <= SPREAD_RADIUS; i++) {
        for (int j = -SPREAD_RADIUS; j <= SPREAD_RADIUS; j++) {
            int2 adjacentPos = screenPos + int2(i, j);
            if (adjacentPos.x > screenDimension[0] || adjacentPos.x < 0 || adjacentPos.y > screenDimension[1] || adjacentPos.y < 0)
                continue;
            // normal
            float3 adjacentNorm = gNormalTex[adjacentPos].xyz * 2.0f - 1.0f;
            if (all(normal == adjacentNorm)) {
                // get SSR color and spread
                float4 ssrRadiance = gColorTexSSR[adjacentPos];
                spreadNum++;
                spreadedIndirect += ssrRadiance.xyz;
            }
        }
    }*/
    return float4(spreadedIndirect / spreadNum, 1.f);
}