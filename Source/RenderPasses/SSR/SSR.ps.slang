import Scene.Camera.Camera;
import Utils.Math.MatrixUtils;
#ifndef SAMPLE_NUM
#define SAMPLE_NUM 1
#endif

#ifndef VISUALIZE_RAY
#define VISUALIZE_RAY 0
#endif

#ifndef REFLECT
#define REFLECT 0
#endif

#ifndef Z_THICKNESS
#define Z_THICKNESS 0.01
#endif

#ifndef TIME_INTERPOLATION
#define TIME_INTERPOLATION 1
#endif
cbuffer PerFrameCB
{
    Camera gCamera;
    uint2 screenDimension;
    uint frameCnt;
    float hysteresis;
    float4x4 prevInvViewProj;
}

SamplerState gTextureSampler;

Texture2D gDepthTex;
Texture2D gNormalTex;
Texture2D gColorTex;
Texture2D gPrevColorTex;

struct Result
{
    bool isHit = false;

    int2 UV; // screen
    float3 position;
#if VISUALIZE_RAY
    float depth = 0.f;
    float3 outputBuffer;
    bool onLine = false;
#endif
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

float getNdcDepth(float3 csCurPnt) {
    float4 ndcCurPnt = mul(gCamera.data.projMat, float4(csCurPnt, 1.f));
    ndcCurPnt /= ndcCurPnt.w;
    return ndcCurPnt.z;
}

#if VISUALIZE_RAY
Result rayMarching(float3 csOrig, float3 csDir, float stride, int2 texC) {
#else
Result rayMarching(float3 csOrig, float3 csDir, float stride) { // clip space coordinate
#endif
    Result result;
    float maxDistance = 2;
    float rayLength = maxDistance;
    if (csOrig.z + csDir.z * maxDistance > -gCamera.data.nearZ) {
        rayLength = (-gCamera.data.nearZ - csOrig.z) / csDir.z;
    }
    float3 csEndPoint = csOrig + csDir * rayLength;
    
    // clip space
    float4 H0 = mul(gCamera.data.projMat, float4(csOrig, 1.f));
    float4 H1 = mul(gCamera.data.projMat, float4(csEndPoint, 1.f));
    // ndc
    H0 /= H0.w;
    H1 /= H1.w;
    // screen space
    float2 P0 = (H0.xy + 1.0f) / 2.0f;
    float2 P1 = (H1.xy + 1.0f) / 2.0f;
    P0.y = 1.f - P0.y;
    P1.y = 1.f - P1.y;
    P1 += distance(P0, P1) < 0.01 ? 0.01 : 0.0f;
    P0 *= screenDimension; P1 *= screenDimension;
    float2 delta = P1 - P0;

    bool permute = false;
    if (abs(delta.x) < abs(delta.y)) {
        permute = true;
        delta = delta.yx; P0 = P0.yx; P1 = P1.yx;
    }
    float stepDir = sign(delta.x);
    float invdx = stepDir / delta.x;
    float2 dP = float2(stepDir, delta.y * invdx);
    float dNdcZ = (H1.z - H0.z) * invdx;

    dP *= stride; dNdcZ *= stride;

    int Step = 0;
    int MaxStep = 2000;
    float ndcZ = H0.z;
    float2 P = P0;

    P += dP; ndcZ += dNdcZ;
    float prevZMaxEstimate = ndcZ + 0.5 * dNdcZ; // getNdcDepth((dQ * 0.5 + Q) / (dk * 0.5 + k));
    P += dP; ndcZ += dNdcZ;
#if VISUALIZE_RAY
    result.outputBuffer = float3(0.f, 0.f, 0.f);
#endif
    for (; Step < MaxStep; Step++, P += dP, ndcZ += dNdcZ)
    {
        result.UV = permute ? P.yx : P;
        float2 rayZMinMax = float2(
            prevZMaxEstimate,  // rayZMin
            ndcZ + 0.5 * dNdcZ // rayZMax = getNdcDepth((dQ * 0.5 + Q) / (dk * 0.5 + k)) 
        );
        prevZMaxEstimate = rayZMinMax.y;
        if (rayZMinMax.x > rayZMinMax.y)
            rayZMinMax.yx = rayZMinMax.xy; // swap
            
        if (result.UV.x > screenDimension[0] || result.UV.x < 0 || result.UV.y > screenDimension[1] || result.UV.y < 0)
            break;
        float ndcZMin = gDepthTex[result.UV].r;
        float ndcZMax = ndcZMin + Z_THICKNESS;
        result.isHit = rayZMinMax.y >= ndcZMin && rayZMinMax.x <= ndcZMax;
#if VISUALIZE_RAY
        result.onLine = all(result.UV == texC);
        // 如果步进到的像素是当前像素
        if (result.onLine) {
            // 输出参数
            if (rayZMinMax.x > ndcZMax) { // 光线位于表面后方
                result.outputBuffer = float3(0.f, rayZMinMax.x, ndcZMax);
            } else { // 光线位于表面前方
                result.outputBuffer = float3(rayZMinMax.x, 0.f, ndcZMax);
            }
            if (result.isHit) // 相交
                result.outputBuffer = float3(1.f, 1.f, 0.f);
            // 终止步进
            break;
        }
#endif
        if (result.isHit)
            break;
    }
    return result;
}

float4 main(float2 texC: TEXCOORD) : SV_TARGET0
{
    if (gDepthTex.SampleLevel(gTextureSampler, texC, 0).r >= 1)
    {
        return gColorTex.Sample(gTextureSampler, texC);
    }
    // 计算光线起点的clip space坐标
#if VISUALIZE_RAY
    float2 texC_orig = texC;
    texC = float2(0.5, 0.5);
#endif
    float3 posW = getPosition(texC).xyz;
    float4 csOrigin = mul(gCamera.data.viewMat, float4(posW, 1.f));
    csOrigin /= csOrigin.w;

    float3 normal = normalize(gNormalTex.Sample(gTextureSampler, texC).xyz * 2.0f - 1.0f);
    float3 viewDir = normalize(posW - gCamera.data.posW);
#if VISUALIZE_RAY
    float s = InitRand(posW.xy + posW.z);
#else
    float s = InitRand(texC + float2(frameCnt / 233.f, frameCnt / 10086.f) );
#endif
    float3 indirect = float3(0.f, 0.f, 0.f);
    for (int i = 0; i < SAMPLE_NUM; i++) {
        s += float(i);
        float pdf = 0.0;
#if REFLECT
        float3 reflectDir = viewDir - 2.f * normal * dot(normal, viewDir); // reflect(viewDir, normal); //   // 
#else
        float3 reflectDir = SampleDirection(normal, s, pdf);
#endif
        float4 csDirection = mul(gCamera.data.viewMat, float4(reflectDir, 0.f));

#if VISUALIZE_RAY
        Result r = rayMarching(csOrigin.xyz, csDirection.xyz, 1.f, texC_orig * screenDimension);
#else
        Result r = rayMarching(csOrigin.xyz, csDirection.xyz, 1.0f);
#endif
        if (r.isHit) {
            indirect.xyz += gColorTex[r.UV].xyz;
        }
#if VISUALIZE_RAY
        return float4(r.outputBuffer, 1.f);
#endif // VISUALIZE_RAY
#if REFLECT
        return float4(indirect, 1.f);
#endif // REFLECT
    }
    float3 res = indirect / float(SAMPLE_NUM);
#if TIME_INTERPOLATION
    // 变换到上一帧的ndc
    float4 prevNdc = mul(prevInvViewProj, float4(posW, 1.f));
    prevNdc /= prevNdc.w;
    float2 prevScreenPos = prevNdc.xy * float2(0.5, -0.5) + 0.5;
    float3 prevColor = gPrevColorTex.Sample(gTextureSampler, prevScreenPos).xyz;
    res = hysteresis * prevColor + (1 - hysteresis) * res;
#endif
    return float4(res, 1.f);
}
