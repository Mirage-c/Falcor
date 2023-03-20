/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "ShadowDepthPass.h"
#include "RenderGraph/RenderPassLibrary.h"

const RenderPass::Info ShadowDepthPass::kInfo { "ShadowDepthPass", "Insert pass description here." };

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(ShadowDepthPass::kInfo, ShadowDepthPass::create);
}

namespace {
    const std::string kShadowDepth = "shadowDepth";
    const std::string kDepth = "depth";
    const std::string kShadowPassfile = "RenderPasses/ShadowDepthPass/ShadowDepthPass.slang";
}

void ShadowDepthPass::createShadowPassResources()
{
    mShadowPass.mapSize = mMapSize;
    const ResourceFormat depthFormat = ResourceFormat::D32Float;
    // mCsmData.depthBias = 0.005f;

    ResourceFormat colorFormat = ResourceFormat::Unknown;

    Fbo::Desc fboDesc;
    fboDesc.setDepthStencilTarget(depthFormat);
    uint32_t mipLevels = 1;
    mShadowPass.pFbo = Fbo::create2D(mMapSize.x, mMapSize.y, fboDesc, 1, mipLevels);
    mShadowPass.fboAspectRatio = (float)mMapSize.x / (float)mMapSize.y;
    
    mShadowPass.pState = GraphicsState::create();
}

ShadowDepthPass::ShadowDepthPass() : RenderPass(kInfo) {
    mpLightCamera = Camera::create();
}

ShadowDepthPass::SharedPtr ShadowDepthPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pRSM = SharedPtr(new ShadowDepthPass());
    pRSM->createShadowPassResources();
    return pRSM;
}

Dictionary ShadowDepthPass::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection ShadowDepthPass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addInput(kDepth, "Pre-initialized scene depth buffer used for SDSM");
    const uint2 dim = { 512, 512 }; // TODO
    reflector.addOutput(kShadowDepth, "Depth buffer").format(ResourceFormat::D32Float).texture2D(0, 0);
    return reflector;
}

void camClipSpaceToWorldSpace(const Camera* pCamera, float3 viewFrustum[8], float3& center, float& radius)
{
    float3 clipSpace[8] =
    {
        float3(-1.0f, 1.0f, 0),
        float3(1.0f, 1.0f, 0),
        float3(1.0f, -1.0f, 0),
        float3(-1.0f, -1.0f, 0),
        float3(-1.0f, 1.0f, 1.0f),
        float3(1.0f, 1.0f, 1.0f),
        float3(1.0f, -1.0f, 1.0f),
        float3(-1.0f, -1.0f, 1.0f),
    };

    rmcv::mat4 invViewProj = pCamera->getInvViewProjMatrix();
    center = float3(0, 0, 0);

    for (uint32_t i = 0; i < 8; i++)
    {
        float4 crd = invViewProj * float4(clipSpace[i], 1);
        viewFrustum[i] = float3(crd) / crd.w;
        center += viewFrustum[i];
    }

    center *= (1.0f / 8.0f);

    // Calculate bounding sphere radius
    radius = 0;
    for (uint32_t i = 0; i < 8; i++)
    {
        float d = glm::length(center - viewFrustum[i]);
        radius = std::max(d, radius);
    }
}

static void createShadowMatrix(const DirectionalLight* pLight, const float3& center, float radius, rmcv::mat4& shadowVP)
{
    rmcv::mat4 view = rmcv::lookAt(center, center + pLight->getWorldDirection(), float3(0, 1, 0));
    rmcv::mat4 proj = rmcv::ortho(-radius, radius, -radius, radius, -radius, radius);

    shadowVP = proj * view;
}

static void createShadowMatrix(const PointLight* pLight, const float3& center, float radius, float fboAspectRatio, rmcv::mat4& shadowVP)
{
    const float3 lightPos = pLight->getWorldPosition();
    const float3 lookat = pLight->getWorldDirection() + lightPos;
    float3 up(0, 1, 0);
    if (abs(glm::dot(up, pLight->getWorldDirection())) >= 0.95f)
    {
        up = float3(1, 0, 0);
    }

    rmcv::mat4 view = rmcv::lookAt(lightPos, lookat, up);
    float distFromCenter = glm::length(lightPos - center);
    float nearZ = std::max(0.1f, distFromCenter - radius);
    float maxZ = std::min(radius * 2, distFromCenter + radius);
    float angle = pLight->getOpeningAngle() * 2;
    rmcv::mat4 proj = rmcv::perspective(angle, fboAspectRatio, nearZ, maxZ);

    shadowVP = proj * view;
}

static void createShadowMatrix(const Light* pLight, const float3& center, float radius, float fboAspectRatio, rmcv::mat4& shadowVP)
{
    switch (pLight->getType())
    {
    case LightType::Directional:
        return createShadowMatrix((DirectionalLight*)pLight, center, radius, shadowVP);
    case LightType::Point:
        return createShadowMatrix((PointLight*)pLight, center, radius, fboAspectRatio, shadowVP);
    default:
        FALCOR_UNREACHABLE();
    }
}

void getCascadeCropParams(const float3 crd[8], const rmcv::mat4& lightVP, float4& scale, float4& offset)
{
    // Transform the frustum into light clip-space and calculate min-max
    float4 maxCS(-1, -1, 0, 1);
    float4 minCS(1, 1, 1, 1);
    for (uint32_t i = 0; i < 8; i++)
    {
        float4 c = lightVP * float4(crd[i], 1.0f);
        c /= c.w;
        maxCS = glm::max(maxCS, c);
        minCS = glm::min(minCS, c);
    }

    float4 delta = maxCS - minCS;
    scale = float4(2, 2, 1, 1) / delta;

    offset.x = -0.5f * (maxCS.x + minCS.x) * scale.x;
    offset.y = -0.5f * (maxCS.y + minCS.y) * scale.y;
    offset.z = -minCS.z * scale.z;

    scale.w = 1;
    offset.w = 0;
}

void ShadowDepthPass::partitionCascades(const Camera* pCamera, const float2& distanceRange)
{
    struct
    {
        float3 crd[8];
        float3 center;
        float radius;
    } camFrustum;

    camClipSpaceToWorldSpace(pCamera, camFrustum.crd, camFrustum.center, camFrustum.radius);

    // Create the global shadow space
    createShadowMatrix(mpLight.get(), camFrustum.center, camFrustum.radius, mShadowPass.fboAspectRatio, globalMat);

    float nearPlane = pCamera->getNearPlane();
    float farPlane = pCamera->getFarPlane();
    float depthRange = farPlane - nearPlane;

    float nextCascadeStart = distanceRange.x;

    for (uint32_t c = 0; c < 1; c++)
    {
        float cascadeStart = nextCascadeStart;
        nextCascadeStart = cascadeStart + (distanceRange.y - distanceRange.x) / float(1);
        // If we blend between cascades, we need to expand the range to make sure we will not try to read off the edge of the shadow-map
        float blendCorrection = 0.f;
        float cascadeEnd = nextCascadeStart + blendCorrection;
        nextCascadeStart -= blendCorrection;

        // Calculate the cascade distance in camera-clip space(Where the clip-space range is [0, farPlane])
        float camClipSpaceCascadeStart = glm::lerp(nearPlane, farPlane, cascadeStart);
        float camClipSpaceCascadeEnd = glm::lerp(nearPlane, farPlane, cascadeEnd);

        //Convert to ndc space [0, 1]
        float projTermA = farPlane / (nearPlane - farPlane);
        float projTermB = (-farPlane * nearPlane) / (farPlane - nearPlane);
        float ndcSpaceCascadeStart = (-camClipSpaceCascadeStart * projTermA + projTermB) / camClipSpaceCascadeStart;
        float ndcSpaceCascadeEnd = (-camClipSpaceCascadeEnd * projTermA + projTermB) / camClipSpaceCascadeEnd;

        // Calculate the cascade frustum
        float3 cascadeFrust[8];
        for (uint32_t i = 0; i < 4; i++)
        {
            float3 edge = camFrustum.crd[i + 4] - camFrustum.crd[i];
            float3 start = edge * cascadeStart;
            float3 end = edge * cascadeEnd;
            cascadeFrust[i] = camFrustum.crd[i] + start;
            cascadeFrust[i + 4] = camFrustum.crd[i] + end;
        }

        getCascadeCropParams(cascadeFrust, globalMat, cascadeScale, cascadeOffset);
        // logInfo("### [{}] cascadeScale: {},{},{},{} ### cascadeOffset: {},{},{},{}", c, mCsmData.cascadeScale[c].x, mCsmData.cascadeScale[c].y, mCsmData.cascadeScale[c].z, mCsmData.cascadeScale[c].w, mCsmData.cascadeOffset[c].x, mCsmData.cascadeOffset[c].y, mCsmData.cascadeOffset[c].z, mCsmData.cascadeOffset[c].w);
    }
}

void ShadowDepthPass::createSdsmData(Texture::SharedPtr pTexture)
{
    FALCOR_ASSERT(pTexture);
    // Only create a new technique if it doesn't exist or the dimensions changed
    if (mSdsmData.minMaxReduction)
    {
        if (mSdsmData.width == pTexture->getWidth() && mSdsmData.height == pTexture->getHeight() && mSdsmData.sampleCount == pTexture->getSampleCount())
        {
            return;
        }
    }
    mSdsmData.width = pTexture->getWidth();
    mSdsmData.height = pTexture->getHeight();
    mSdsmData.sampleCount = pTexture->getSampleCount();
    mSdsmData.minMaxReduction = ParallelReduction::create(ParallelReduction::Type::MinMax, mSdsmData.readbackLatency, mSdsmData.width, mSdsmData.height, mSdsmData.sampleCount);
}

void ShadowDepthPass::reduceDepthSdsmMinMax(RenderContext* pRenderCtx, const Camera* pCamera, Texture::SharedPtr pDepthBuffer)
{
    if (pDepthBuffer == nullptr)
    {
        return;
    }

    createSdsmData(pDepthBuffer);
    float2 distanceRange = float2(mSdsmData.minMaxReduction->reduce(pRenderCtx, pDepthBuffer));

    // Convert to linear
    rmcv::mat4 camProj = pCamera->getProjMatrix();
    distanceRange = camProj[2][2] - distanceRange * camProj[3][2];
    distanceRange = camProj[2][3] / distanceRange;
    distanceRange = (distanceRange - pCamera->getNearPlane()) / (pCamera->getFarPlane() - pCamera->getNearPlane());
    distanceRange = glm::clamp(distanceRange, float2(0), float2(1));
    mSdsmData.sdsmResult = distanceRange;
}

void ShadowDepthPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpLight || !mpScene) return;
    // setup fbo
    Texture::SharedPtr pShadowDepth = renderData.getTexture(kShadowDepth);
    Texture::SharedPtr pDepth = renderData.getTexture(kDepth);


    mShadowPass.pFbo->attachDepthStencilTarget(pShadowDepth);    
    mShadowPass.pState->setFbo(mShadowPass.pFbo);
    pRenderContext->clearDsv(pShadowDepth->getDSV().get(), 1, 0);

    const auto pCamera = mpScene->getCamera().get();
    const float4 clearColor(0);
    pRenderContext->clearFbo(mShadowPass.pFbo.get(), clearColor, 1, 0, FboAttachmentType::All);

    // Calc the bounds
    reduceDepthSdsmMinMax(pRenderContext, pCamera, pDepth);
    float2 distanceRange = mSdsmData.sdsmResult;


    GraphicsState::Viewport VP;
    VP.originX = 0;
    VP.originY = 0;
    VP.minDepth = 0;
    VP.maxDepth = 1;
    VP.height = mShadowPass.mapSize.x;
    VP.width = mShadowPass.mapSize.y;

    // Set shadow pass state
    mShadowPass.pState->setViewport(0, VP);
    // auto pCB = mShadowPass.pVars->getParameterBlock(mPerLightCbLoc);
    // pCB->setBlob(&mCsmData, 0, sizeof(mCsmData));
    partitionCascades(pCamera, distanceRange);
    mShadowPass.pVars["PerFrameCB"]["globalMat"] = globalMat;
    mShadowPass.pVars["PerFrameCB"]["cascadeScale"] = cascadeScale;
    mShadowPass.pVars["PerFrameCB"]["cascadeOffset"] = cascadeOffset;

    InternalDictionary& dict = renderData.getDictionary();
    dict["globalMat"] = globalMat;
    dict["cascadeScale"] = cascadeScale;
    dict["cascadeOffset"] = cascadeOffset;
   // logInfo("mSdsmData.sdsmResult: {}, {}; scale: ({}, {}), offset: ({}. {})", mSdsmData.sdsmResult.x, mSdsmData.sdsmResult.y, cascadeScale.x, cascadeScale.y, cascadeOffset.x, cascadeOffset.y);

    mpScene->rasterize(pRenderContext, mShadowPass.pState.get(), mShadowPass.pVars.get(), RasterizerState::CullMode::Back);
}

void ShadowDepthPass::setLight(const Light::SharedConstPtr& pLight)
{
    mpLight = pLight;
    logInfo("[CSM::setLight()] lightType: '{}'", mpLight->getLightType());
}

void ShadowDepthPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;

    setLight(mpScene && mpScene->getLightCount() ? mpScene->getLight(0) : nullptr);

    if (mpScene)
    {

        // Create the shadows program
        GraphicsProgram::Desc desc;
        desc.addShaderModules(pScene->getShaderModules());
        desc.addTypeConformances(pScene->getTypeConformances());
        desc.addShaderLibrary(kShadowPassfile).vsEntry("vsMain").psEntry("psMain");
        mShadowPass.pProgram = GraphicsProgram::create(desc, mpScene->getSceneDefines());
        mShadowPass.pState->setProgram(mShadowPass.pProgram);
        logInfo("[ShadowDepthPass::setScene] ShadowDepthPass slang file: {}", kShadowPassfile);

        // mShadowPass.pProgram->addTypeConformance(mpScene->getTypeConformances());
        mShadowPass.pVars = GraphicsVars::create(mShadowPass.pProgram->getReflector());

        const auto& pReflector = mShadowPass.pVars->getReflection();
        const auto& pDefaultBlock = pReflector->getDefaultParameterBlock();
    }
    else
    {
        mShadowPass.pVars = nullptr;
    }
}


void ShadowDepthPass::renderUI(Gui::Widgets& widget)
{
}

