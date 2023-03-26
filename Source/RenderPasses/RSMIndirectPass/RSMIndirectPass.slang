import Utils.Sampling.TinyUniformSampleGenerator;

cbuffer PerFrameCB
{
    float4x4 globalMat;
    float4 cascadeScale;
    float4 cascadeOffset;
    uint2 screenDimension;
    uint2 shadowMapDimension;
};

Texture2D gNormalTex;
Texture2D gWorldPosTex;

Texture2D rNormalTex;
Texture2D rFluxTex;
Texture2D rWorldPosTex;

Texture2D samplesTex;

SamplerState gSampler;

float4 main(float2 texC: TEXCOORD) : SV_TARGET0
{
    float2 uv = texC * screenDimension;
    float3 posW = gWorldPosTex[uv].xyz;
    float3 norm = gNormalTex[uv].xyz;
    float4 texPos = mul(globalMat, float4(posW, 1.f));
    texPos /= texPos.w;

    // scale and offset
    float3 scale = cascadeScale.xyz;
    scale.xy *= 0.5;
    float3 offset = cascadeOffset.xyz;
    // offset.x += 0.1;
    // offset.y -= 0.15;
    offset.xy = (offset.xy + 1) * 0.5;
    texPos.xyz = texPos.xyz * scale + offset;
    texPos.y = 1 - texPos.y;

    // float2 invSize = 1.0f / shadowMapDimension;
    // texPos.xy = 2 * texPos.xy + invSize - 1;

    float3 indirect = float3(0.f, 0.f, 0.f);
    float sampleRadius = 300.0f;
    int NUM_SAMPLES = 64;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float3 s = samplesTex[int2(i, 0)].xyz;
        float2 bias = s.xy;
        float weight = s.z;

        float2 coords = texPos.xy * shadowMapDimension + bias * sampleRadius;
        // coords = float2(900, 900);
        float3 vplPos = rWorldPosTex[coords].xyz;
        float3 vplNorm = rNormalTex[coords].xyz;
        float3 vplFlux = rFluxTex[coords].xyz;
        // float3 vplPos = rWorldPosTex.Sample(gSampler, coords).xyz;
        // float3 vplNorm = rNormalTex.Sample(gSampler, coords).xyz;
        // float3 vplFlux = rFluxTex.Sample(gSampler, coords).xyz;
        return float4(vplNorm, 1.f);

        float3 result = vplFlux * max(0.f, dot(vplNorm, normalize(posW - vplPos))) * max(0.f, dot(norm, normalize(vplPos - posW)));
        result *= weight * weight;
        result *= (1.f / NUM_SAMPLES);
        // result = float3(coords.x, 1.f, 1.f);
        indirect += result;
    }
    return float4(indirect * 30.f, 1.f); // clamp(indirect, 0.f, 1.f)
}