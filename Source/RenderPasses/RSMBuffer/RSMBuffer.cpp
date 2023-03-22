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
#include "RSMBuffer.h"
#include "RenderGraph/RenderPassLibrary.h"

const RenderPass::Info RSMBuffer::kInfo { "RSMBuffer", "Insert pass description here." };

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(RSMBuffer::kInfo, RSMBuffer::create);
}

namespace {
    const std::string kDepth = "shadowDepth";
    const std::string kPosW = "worldPosition";
    const std::string kNorm = "normal";
    const std::string kColor = "color";
    const std::string kShadowPassfile = "RenderPasses/RSMBuffer/ShadowPass.slang";
}

void RSMBuffer::createShadowPassResources()
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
    mShadowPass.pState->setDepthStencilState(nullptr);
    mShadowPass.pState->setFbo(mShadowPass.pFbo);

    RasterizerState::Desc rsDesc;
    rsDesc.setDepthClamp(true);
    rsState = RasterizerState::create(rsDesc);
    mShadowPass.pState->setRasterizerState(rsState);
}

RSMBuffer::RSMBuffer() : RenderPass(kInfo) {
    mpLightCamera = Camera::create();

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point).setAddressingMode(Sampler::AddressMode::Border, Sampler::AddressMode::Border, Sampler::AddressMode::Border).setBorderColor(float4(1.0f));
    samplerDesc.setLodParams(0.f, 0.f, 0.f);
    samplerDesc.setComparisonMode(Sampler::ComparisonMode::LessEqual);
    mShadowPass.pPointCmpSampler = Sampler::create(samplerDesc);
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    mShadowPass.pLinearCmpSampler = Sampler::create(samplerDesc);
    samplerDesc.setComparisonMode(Sampler::ComparisonMode::Disabled);
    samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    samplerDesc.setLodParams(-100.f, 100.f, 0.f);
}

RSMBuffer::SharedPtr RSMBuffer::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pRSM = SharedPtr(new RSMBuffer());
    pRSM->createShadowPassResources();

    logInfo("RSMBuffer Created.");
    return pRSM;
}

Dictionary RSMBuffer::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection RSMBuffer::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addInput(kDepth, "Depth buffer").format(ResourceFormat::D32Float).bindFlags(Resource::BindFlags::DepthStencil).texture2D(0,0);
    reflector.addOutput(kPosW, "World space position").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    reflector.addOutput(kNorm, "World space normal").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    reflector.addOutput(kColor, "Color").format(ResourceFormat::RGBA32Float).texture2D(0, 0); // .bindFlags(Resource::BindFlags::RenderTarget).texture2D(dim.x, dim.y)
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

void RSMBuffer::partitionCascades(const Camera* pCamera, const float2& distanceRange)
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

void RSMBuffer::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpLight || !mpScene) return;
    InternalDictionary& dict = renderData.getDictionary();
    // setup fbo
    // Texture::SharedPtr pDepth;
    Texture::SharedPtr pPreDepth = renderData.getTexture(kDepth);
    // pRenderContext->copyResource(pDepth.get(), pPreDepth.get());
    Texture::SharedPtr pPosW = renderData.getTexture(kPosW);
    Texture::SharedPtr pNorm = renderData.getTexture(kNorm);
    Texture::SharedPtr pColor = renderData.getTexture(kColor);

    // mShadowPass.pFbo->attachColorTarget(pDepth, 0);
    mShadowPass.pFbo->attachDepthStencilTarget(pPreDepth);
    mShadowPass.pFbo->attachColorTarget(pPosW, 0);
    mShadowPass.pFbo->attachColorTarget(pNorm, 1);
    mShadowPass.pFbo->attachColorTarget(pColor, 2);

    // pRenderContext->clearRtv(mShadowPass.pFbo->getRenderTargetView(0).get(), float4(0));
    // pRenderContext->clearRtv(mShadowPass.pFbo->getRenderTargetView(0).get(), float4(1));
    // pRenderContext->clearRtv(mShadowPass.pFbo->getRenderTargetView(0).get(), float4(2));

    const auto pCamera = mpScene->getCamera().get();
    //const auto pCamera = mpCsmSceneRenderer->getScene()->getActiveCamera().get();

    const float4 clearColor(0);
    pRenderContext->clearFbo(mShadowPass.pFbo.get(), clearColor, 1, 0, FboAttachmentType::All);

    // Calc the bounds
    float2 distanceRange = float2(0, 1);


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
    globalMat = dict["globalMat"];
    cascadeOffset = dict["cascadeOffset"];
    cascadeScale = dict["cascadeScale"];
    mShadowPass.pVars["PerFrameCB"]["globalMat"] = globalMat;
    mShadowPass.pVars["PerFrameCB"]["cascadeScale"] = cascadeScale;
    mShadowPass.pVars["PerFrameCB"]["cascadeOffset"] = cascadeOffset;

    // logInfo("scale: ({}, {}), offset: ({}. {})",cascadeScale.x, cascadeScale.y, cascadeOffset.x, cascadeOffset.y);

    mpScene->rasterize(pRenderContext, mShadowPass.pState.get(), mShadowPass.pVars.get(), rsState, rsState);
}

void RSMBuffer::setLight(const Light::SharedConstPtr& pLight)
{
    mpLight = pLight;
    logInfo("[CSM::setLight()] lightType: '{}'", mpLight->getLightType());
}

void RSMBuffer::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
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
        logInfo("[RSMBuffer::setScene] RSMBuffer slang file: {}", kShadowPassfile);

        // mShadowPass.pProgram->addTypeConformance(mpScene->getTypeConformances());
        mShadowPass.pVars = GraphicsVars::create(mShadowPass.pProgram->getReflector());

        const auto& pReflector = mShadowPass.pVars->getReflection();
        const auto& pDefaultBlock = pReflector->getDefaultParameterBlock();
        mPerLightCbLoc = pDefaultBlock->getResourceBinding("PerFrameCB");
    }
    else
    {
        mShadowPass.pVars = nullptr;
        mPerLightCbLoc = {};
    }
}


void RSMBuffer::renderUI(Gui::Widgets& widget)
{
}

