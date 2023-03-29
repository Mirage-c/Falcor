import Scene.Camera.Camera;
import Utils.Math.MatrixUtils;
import SSDOData;

cbuffer StaticCB
{
    SSDOData gData;
}

cbuffer PerFrameCB
{
    Camera gCamera;
}

SamplerState gNoiseSampler;
SamplerState gTextureSampler;

Texture2D gDepthTex;
Texture2D gNormalTex;
Texture2D gNoiseTex;
Texture2D gColorTex;

float4 getPosition(float2 uv)
{
    float4 pos;
    pos.x = uv.x * 2.0f - 1.0f;
    pos.y = (1.0f - uv.y) * 2.0f - 1.0f;
#ifdef FALCOR_FLIP_Y
    // NDC Y is top-to-bottom
    pos.y = -pos.y;
#endif
    pos.z = gDepthTex.SampleLevel(gTextureSampler, uv, 0).r;
    pos.w = 1.0f;

    float4 posW = mul(gCamera.data.invViewProj, pos);
    posW /= posW.w;

    return posW;
}

float4 main(float2 texC : TEXCOORD) : SV_TARGET0
{
    if (gDepthTex.SampleLevel(gTextureSampler, texC, 0).r >= 1)
    {
        return gColorTex.Sample(gTextureSampler, texC);
    }

    // Calculate world position of pixel
    float3 posW = getPosition(texC).xyz;
    float3 normal = normalize(gNormalTex.Sample(gTextureSampler, texC).xyz * 2.0f - 1.0f);
    float originDist = length(posW - gCamera.data.posW);
    float3 randDir = gNoiseTex.Sample(gNoiseSampler, texC * gData.noiseScale).xyz * 2.0f - 1.0f;

    float3 tangent = normalize(randDir - normal * dot(randDir, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 tbn = float3x3FromCols(tangent, bitangent, normal);

    float3 indirect = float3(0.f, 0.f, 0.f);
    for (uint i = 0; i < gData.kernelSize; i++)
    {
        // Orient sample
        float3 kernelPos = mul(tbn, gData.sampleKernel[i].xyz);

        // Calculate sample world space pos
        float3 samplePosW = posW + (kernelPos * gData.radius);
        float sampleDepth = length(samplePosW - gCamera.data.posW);

        // Get screen space pos of sample
        float4 samplePosProj = mul(gCamera.data.viewProjMat, float4(samplePosW, 1.0f));
        samplePosProj /= samplePosProj.w;

        float2 sampleUV = saturate(float2(samplePosProj.x, -samplePosProj.y) * 0.5f + 0.5f);
        float isOccluded = 0;
        float sampleZ = samplePosProj.z;
        float shadingPointZ = gDepthTex.SampleLevel(gTextureSampler, sampleUV, 0).r;
        isOccluded = step(shadingPointZ, sampleZ);
        // isOccluded = step();
        // float sceneDepth = length(getPosition(sampleUV).xyz - gCamera.data.posW);
        // float rangeCheck = step(abs(sampleDepth - sceneDepth), radius);
        // isOccluded = step(sceneDepth, sampleDepth) * rangeCheck;
        if (isOccluded == 1) {
            // return float4(isOccluded, 0.f, 0.f, 1.f);
            float3 res = gColorTex.Sample(gTextureSampler, sampleUV).rgb;
            float3 vplNorm = normalize((gNormalTex.Sample(gTextureSampler, sampleUV).xyz - 0.5) * 2.0);
            float3 vplPos = getPosition(sampleUV).xyz;
            res *= max(0.f, dot(vplNorm, normalize(posW - vplPos)));
            res *= max(0.f, dot(normal, normalize(vplPos - posW)));
            // float weight = dot(sampleUV - texC, sampleUV - texC);
            // res *= weight * weight;
            indirect += res;
        }
    }
    indirect *= (1.f / gData.kernelSize) * 10;
    float3 color = float3(0.f, 0.f, 0.f);
    color = gColorTex.Sample(gTextureSampler, texC).rgb;
    return float4(indirect + color, 1.f);
}
