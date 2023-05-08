import Scene.Camera.Camera;
import Utils.Math.MatrixUtils;

// cbuffer StaticCB
// {
// }

cbuffer PerFrameCB
{
    Camera gCamera;
    uint2 screenDimension;
}

// SamplerState gNoiseSampler;
// Texture2D gNoiseTex;
SamplerState gTextureSampler;

Texture2D gDepthTex;
Texture2D gNormalTex;
Texture2D gColorTex;

struct Result
{
    bool isHit = false;

    float2 UV;
    float3 position;

    float rayLength;
};

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

float getViewZ(float2 uv) {
    float4 posW = getPosition(uv);
    float4 posV = mul(gCamera.data.viewMat, posW);
    posV /= posV.w;
    return posV.z;
}

Result rayMarching(float3 startPoint, float3 direction, float stride) { // clip space coordinate
    float maxDistance = 0.5;
    float rayLength = maxDistance;
    if (startPoint.z + direction.z * maxDistance > -gCamera.data.nearZ) {
        rayLength = (-gCamera.data.nearZ - startPoint.z) / direction.z;
    }
    float3 endPoint = startPoint + direction * rayLength;
    // project back to camera, ndc
    float4 H0 = mul(gCamera.data.projMat, float4(startPoint, 1.f));
    float4 H1 = mul(gCamera.data.projMat, float4(endPoint, 1.f));
    float k0 = 1.0f / H0.w;
    float k1 = 1.0f / H1.w;
    float3 Q0 = startPoint.xyz * k0;
    float3 Q1 = endPoint.xyz * k1;
    // convert to screen space
    H0 /= H0.w;
    H1 /= H1.w;
    float2 P0 = (H0.xy + 1.0f) / 2.0f;
    float2 P1 = (H1.xy + 1.0f) / 2.0f;
    P0.y = 1.f - P0.y;
    P1.y = 1.f - P1.y;
    // P1 += distance(P0, P1) < 0.01 ? 0.01 : 0.0f;
    P0 *= screenDimension; P1 *= screenDimension;
    float2 delta = P1 - P0;

    bool permute = false;
    if (abs(delta.x) < abs(delta.y)) {
        permute = true;
        delta = delta.yx; P0 = P0.yx; P1 = P1.yx;
    }
    float stepDir = sign(delta.x);
    float invdx = stepDir / delta.x;
    float3 dQ = (Q1 - Q0) * invdx;
    float dk = (k1 - k0) * invdx;
    float2 dP = float2(stepDir, delta.y * invdx);

    dP *= stride; dQ *= stride; dk *= stride;

    P0 += dP; Q0 += dQ; k0 += dk;

    int Step = 0;
    int MaxStep = 5000;
    float k = k0;
    float EndX = P1.x * stepDir;
    float3 Q = Q0;
    float prevZMaxEstimate = startPoint.z;

    Result result;
    result.rayLength = rayLength;
    for (float2 P = P0; Step < MaxStep; Step++, P += dP, Q.z += dQ.z, k += dk)
    {
        result.UV = permute ? P.yx : P;
        float2 Depths;
        Depths.x = prevZMaxEstimate;
        Depths.y = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k);
        prevZMaxEstimate = Depths.y;
        if (Depths.x < Depths.y)
            Depths.xy = Depths.yx;
        if (result.UV.x > screenDimension[0] || result.UV.x < 0 || result.UV.y > screenDimension[1] || result.UV.y < 0)
            break;
        float screenDepth = -gDepthTex[result.UV].r;
        result.isHit = Depths.y <= screenDepth && Depths.x >= screenDepth;
        if (result.isHit)
            break;
    }
    // P0 += 100 * dP;
    // result.isHit = true;
    // result.UV = permute ? P0.yx : P0;
    return result;
}

Result rayMarching2(float3 ori, float3 dir, float stepSize) { // world
    dir = normalize(dir);
    float3 currentCheck = ori + dir;
    Result result;
    result.isHit = false;
    for (int i = 0; i < 20; i++) {
        float4 posH = mul(gCamera.data.viewProjMat, float4(currentCheck, 1.f));
        posH /= posH.w; // ndc
        float2 uv = posH.xy * 0.5 + 0.5;
        // uv.y = 1.f - uv.y;
        result.UV = uv;
        if (result.UV.x > 1.f || result.UV.x < 0.f || result.UV.y > 1.f || result.UV.y < 0.f)
            break;
        if (gDepthTex.Sample(gTextureSampler, result.UV).r < posH.z) {
            result.UV *= screenDimension;
            result.isHit = true;
            break;
        }
    }
    return result;
}

float4 main(float2 texC: TEXCOORD) : SV_TARGET0
{
    if (gDepthTex.SampleLevel(gTextureSampler, texC, 0).r >= 1)
    {
        return gColorTex.Sample(gTextureSampler, texC);
    }

    // Calculate world position of pixel
    float3 posW = getPosition(texC).xyz;
    float3 normal = gNormalTex.Sample(gTextureSampler, texC).xyz;
    normal = normalize(normal * 2.0f - 1.0f);
    float3 viewDir = normalize(posW - gCamera.data.posW);

    // float3 tangent = normalize(randDir - normal * dot(randDir, normal));
    // float3 bitangent = cross(normal, tangent);
    // float3x3 tbn = float3x3FromCols(tangent, bitangent, normal);
    float3 reflectDir = normalize(reflect(viewDir, normal));
    reflectDir = normal;

    // view
    float4 csOrigin = mul(gCamera.data.viewMat, float4(posW, 1.f));
    // return float4(mul(gCamera.data.viewProjMat, float4(posW, 1.f)).z, 0.f, 0.f, 1.f);
    // return float4(-csOrigin.z, 0.f, 0.f, 1.f);

    float4 csDirection = mul(gCamera.data.viewMat, float4(reflectDir, 1.f));
    csOrigin /= csOrigin.w;
    csDirection /= csDirection.w;
    csDirection.xyz = normalize(csDirection.xyz);

    Result r = rayMarching(csOrigin.xyz, csDirection.xyz, 1.0f);
    // Result r = rayMarching2(posW, reflectDir, 1.f);
    float3 indirect = float3(0.f, 0.f, 0.f);
    if (r.isHit) {
        indirect.xyz += gColorTex[r.UV].xyz * 0.5;
        // return float4(gColorTex[texC * screenDimension].xyz, 1);
    }

    // float3 color = float3(0.f, 0.f, 0.f);
    // color = gColorTex.Sample(gTextureSampler, texC).rgb;
    return float4(indirect, 1.f);
}
