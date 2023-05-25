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
    float3 norm = normalize((gNormalTex[uv].xyz - 0.5) * 2.0);
    float4 texPos = mul(globalMat, float4(posW, 1.f));
    texPos /= texPos.w;

    // scale and offset
    float3 scale = cascadeScale.xyz;
    scale.xy *= 0.5;
    float3 offset = cascadeOffset.xyz;
    offset.xy = (offset.xy + 1) * 0.5;
    texPos.xyz = texPos.xyz * scale + offset;
    texPos.y = 1 - texPos.y;

    float3 indirect = float3(0.f, 0.f, 0.f);
    float sampleRadius = 0.1f;
    int NUM_SAMPLES = 512;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float3 s = samplesTex[int2(i, 0)].xyz;
        float2 bias = s.xy;
        float weight = s.z;

        float2 coords = texPos.xy + bias * sampleRadius;
        coords *= shadowMapDimension;
        float3 vplPos = rWorldPosTex[coords].xyz;
        float3 vplNorm = normalize((rNormalTex[coords].xyz - 0.5) * 2.0);
        float3 vplFlux = rFluxTex[coords].xyz;
        // float3 vplPos = rWorldPosTex.Sample(gSampler, coords).xyz;
        // float3 vplNorm = (rNormalTex.Sample(gSampler, coords).xyz- 0.5) * 2.0;
        // float3 vplFlux = rFluxTex.Sample(gSampler, coords).xyz;
        // return float4(vplNorm, 1.f);

        float3 result = vplFlux * 
            max(0.f, dot(vplNorm, normalize(posW - vplPos))) * 
            max(0.f, dot(norm, normalize(vplPos - posW)));
        // result = vplFlux;
        result *= weight * weight;
        // result = float3(coords.x, 1.f, 1.f);
        indirect += result;
    }
    indirect *= (1.f / NUM_SAMPLES);
    return float4(indirect * 0.1f, 1.f); // clamp(indirect, 0.f, 1.f)
}