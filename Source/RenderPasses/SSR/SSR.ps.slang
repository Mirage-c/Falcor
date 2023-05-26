import Scene.Camera.Camera;
import Utils.Math.MatrixUtils;
#define SAMPLE_NUM 1
#define DEBUG 0
#define REFLECT 1

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

    int2 UV; // screen
    float3 position;

    float rayLength;
#if DEBUG
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

#if DEBUG
Result rayMarching(float3 csOrig, float3 csDir, float stride, int2 texC) {
#else
Result rayMarching(float3 csOrig, float3 csDir, float stride) { // clip space coordinate
#endif
    Result result;
    float maxDistance = 2;
    float rayLength = maxDistance;
    result.rayLength = rayLength;
    if (csOrig.z + csDir.z * maxDistance > -gCamera.data.nearZ) {
        rayLength = (-gCamera.data.nearZ - csOrig.z) / csDir.z;
    }
    // csOrig: (0, 0, -0.53)
    float3 csEndPoint = csOrig + csDir * rayLength; // (0.296, 0, -0.1)
    // project back to camera, ndc
    float4 H0 = mul(gCamera.data.projMat, float4(csOrig, 1.f));
    float4 H1 = mul(gCamera.data.projMat, float4(csEndPoint, 1.f));
    float k0 = 1.0f / H0.w; // 1.885
    float k1 = 1.0f / H1.w; // 10.0
    float3 Q0 = csOrig.xyz * k0;
    float3 Q1 = csEndPoint.xyz * k1;
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
    int Step = 0;
    int MaxStep = 2000;
    float k = k0;
    float EndX = P1.x * stepDir;
    float3 Q = Q0;
    P0 += dP; Q += dQ; k += dk;

    float prevZMaxEstimate = getNdcDepth((dQ * 0.5 + Q) / (dk * 0.5 + k));
    P0 += dP; Q += dQ; k += dk;
    for (float2 P = P0; Step < MaxStep; Step++, P += dP, Q += dQ, k += dk)
    {
        result.UV = permute ? P.yx : P;
        float2 Depths;
        Depths.x = prevZMaxEstimate;
        Depths.y = getNdcDepth((dQ * 0.5 + Q) / (dk * 0.5 + k));
        prevZMaxEstimate = Depths.y;
        if (Depths.x < Depths.y)
            Depths.xy = Depths.yx;
        if (result.UV.x > screenDimension[0] || result.UV.x < 0 || result.UV.y > screenDimension[1] || result.UV.y < 0)
            break;
        float screenDepth = gDepthTex[result.UV].r;
        result.isHit = Depths.x >= screenDepth && Depths.y <= screenDepth;
#if DEBUG
        result.onLine = all(result.UV == texC);
        if(result.onLine) {
            result.depth = screenDepth;
            result.depth = Depths.y;
            result.outputBuffer = float3(Depths.y, 0.f, 0.f); 
            break;
        } else {
            result.outputBuffer = float3(0.f, 0.f, 0.f);
        }
        // result.outputBuffer = float3(csOrig); // (1.12, 0, -1.15)
        // result.outputBuffer = float3(csEndPoint);      // (0.296, 0, -0.1)
        // result.outputBuffer = float3(k0, k1, 0.f);     // (1.885, 10, )
        // result.outputBuffer = float3(Q0.z, Q1.z, 0.f); // (1.0, 1.0, )
        // result.outputBuffer = float3(Depths.y, (dQ.z * 0.5 + Q.z), (dk * 0.5 + k));
        // break;
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
#if DEBUG
    float2 texC_orig = texC;
    texC = float2(0.5, 0.5);
#endif
    float3 posW = getPosition(texC).xyz;
    float4 csOrigin = mul(gCamera.data.viewMat, float4(posW, 1.f));
    csOrigin /= csOrigin.w;

    float3 normal = normalize(gNormalTex.Sample(gTextureSampler, texC).xyz * 2.0f - 1.0f);
    float3 viewDir = normalize(posW - gCamera.data.posW); 
    float s = InitRand(texC);
    float3 indirect = float3(0.f, 0.f, 0.f);
    for (int i = 0; i < SAMPLE_NUM; i++) {
        s += float(i);
        float pdf = 0.0;
#if REFLECT
        float3 reflectDir = viewDir - 2.f * normal * dot(normal, viewDir); // reflect(viewDir, normal); //   // 
#else
        float3 reflectDir = SampleDirection(normal, s, pdf);
#endif
        float4x4 spinMat = {
            gCamera.data.viewMat._11, gCamera.data.viewMat._12, gCamera.data.viewMat._13, 0,
            gCamera.data.viewMat._21, gCamera.data.viewMat._22, gCamera.data.viewMat._23, 0,
            gCamera.data.viewMat._31, gCamera.data.viewMat._32, gCamera.data.viewMat._33, 0,
            gCamera.data.viewMat._41, gCamera.data.viewMat._42, gCamera.data.viewMat._43, gCamera.data.viewMat._44,
        };
        float4 csDirection = mul(spinMat, float4(reflectDir, 1.f));
        csDirection /= csDirection.w;
        // float4 csDirection = mul(gCamera.data.viewMat, float4(reflectDir, 0.f));
        csDirection.xyz = normalize(csDirection.xyz);

#if DEBUG
        Result r = rayMarching(csOrigin.xyz, csDirection.xyz, 1.f, texC_orig * screenDimension);
#else
        Result r = rayMarching(csOrigin.xyz, csDirection.xyz, 1.0f);
#endif
        if (r.isHit) {
            indirect.xyz += gColorTex[r.UV].xyz;
        }
#if REFLECT
#if DEBUG
        indirect.xyz = r.outputBuffer;
#endif // DEBUG
        break;
#endif // REFLECT
    }
    return float4(indirect / float(SAMPLE_NUM), 1.f);
}
