import Scene.Camera.Camera;
import Utils.Math.MatrixUtils;
#define SAMPLE_NUM 10
#define DEBUG 0
#define REFLECT 0

cbuffer PerFrameCB
{
    Camera gCamera;
    uint2 screenDimension;
}

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

Result rayMarching(float3 startPoint, float3 direction, float stride) { // clip space coordinate
    Result result;
    float maxDistance = 2;
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
#if DEBUG
    result.position.xyz = float3(0.f);
    result.position.x = startPoint.z + direction.z * maxDistance; // 都是负的！
    result.position.y = startPoint.z; // 0.1
    result.position.z = rayLength; // inf
    return result;
#endif
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
    int MaxStep = 200;
    float k = k0;
    float EndX = P1.x * stepDir;
    float3 Q = Q0;
    float prevZMaxEstimate = startPoint.z;

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
        float screenDepth = gDepthTex[result.UV].r;
        result.isHit = Depths.x >= screenDepth && Depths.y <= screenDepth;
        if (result.isHit)
            break;
    }
    return result;
}

Result rayMarching2(float3 ori, float3 dir, float stepSize) { // world
    dir = normalize(dir) / 10;
    float3 currentCheck = ori + dir;
    Result result;
    result.isHit = false;
    float4 oriPosH = mul(gCamera.data.viewProjMat, float4(ori, 1.f));
    oriPosH /= oriPosH.w;
    float preDepth = oriPosH.z;
    for (int i = 0; i < 1; i++) {
        float4 posH = mul(gCamera.data.viewProjMat, float4(currentCheck, 1.f));
        posH /= posH.w; // ndc
        float2 uv = posH.xy * 0.5 + 0.5;
        uv.y = 1.f - uv.y;
        result.UV = uv;
        if (result.UV.x > 1.f || result.UV.x < 0.f || result.UV.y > 1.f || result.UV.y < 0.f)
            break;
        result.UV *= screenDimension;
        float screenDepth = gDepthTex[result.UV].r; //gDepthTex.Sample(gTextureSampler, result.UV).r;

        float2 depths;
        depths.x = posH.z;
        depths.y = preDepth;

        // debug
        // result.position.x = screenDepth;
        // result.position.yz = depths.xy;
        // return result;

        if (depths.x < depths.y)
            depths.xy = depths.yx; // x >= y

        if (screenDepth <= depths.x) { //  && screenDepth >= depths.y
            result.isHit = true;
            break;
        }

        currentCheck += dir * stepSize;
        preDepth = posH.z;
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
#if DEBUG
    float4 posh = mul(gCamera.data.viewMat, float4(posW, 1.f));
    posh /= posh.w; // startpoint
    posh = mul(gCamera.data.projMat, posh);
    posh /= posh.w;
    float2 newTex = posh.xy * 0.5 + 0.5;
    newTex.y = 1 - newTex.y;
    // return float4(newTex, 0.f, 1.f);
#endif
    float3 normal = normalize(gNormalTex.Sample(gTextureSampler, texC).xyz * 2.0f - 1.0f);
    float3 viewDir = normalize(posW - gCamera.data.posW); 
    float s = InitRand(texC);
    float3 indirect = float3(0.f, 0.f, 0.f);
    for (int i = 0; i < SAMPLE_NUM; i++) {
        s += float(i);
        float pdf = 0.0;
#if REFLECT
        float3 reflectDir = reflect(viewDir, normal); // viewDir - 2.f * normal * dot(normal, viewDir);
#else
        float3 reflectDir = SampleDirection(normal, s, pdf);
#endif
        // view
        float4 csOrigin = mul(gCamera.data.viewMat, float4(posW, 1.f));
        csOrigin /= csOrigin.w;
        float4x4 spinMat = {
            gCamera.data.viewMat._11, gCamera.data.viewMat._12, gCamera.data.viewMat._13, 0,
            gCamera.data.viewMat._21, gCamera.data.viewMat._22, gCamera.data.viewMat._23, 0,
            gCamera.data.viewMat._31, gCamera.data.viewMat._32, gCamera.data.viewMat._33, 0,
            gCamera.data.viewMat._41, gCamera.data.viewMat._42, gCamera.data.viewMat._43, gCamera.data.viewMat._44,
        };
        float4 csDirection = mul(spinMat, float4(reflectDir, 1.f));
        csDirection /= csDirection.w;
        csDirection.xyz = normalize(csDirection.xyz);

        Result r = rayMarching(csOrigin.xyz, csDirection.xyz, 20.0f);
#if DEBUG
        return float4(r.position, 1.f);
#endif
        // Result r = rayMarching2(posW, reflectDir, 1.f);

        if (r.isHit) {
            indirect.xyz += gColorTex[r.UV].xyz;
        }
#if REFLECT
        break;
#endif
    }
    return float4(indirect / float(SAMPLE_NUM), 1.f);
}
