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
#include "RSMIndirectPass.h"
#include "RenderGraph/RenderPassLibrary.h"
#include <glm/gtc/random.hpp>
#include <cmath>

const RenderPass::Info RSMIndirectPass::kInfo { "RSMIndirectPass", "Insert pass description here." };

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(RSMIndirectPass::kInfo, RSMIndirectPass::create);
}


namespace {
    const std::string kShadowDepth = "shadowDepth";
    // RSM
    const std::string kShadowPosW = "shadowWorldPosition";
    const std::string kShadowNorm = "shadowNormal";
    const std::string kShadowColor = "shadowColor";
    // GBuffer
    const std::string kNorm = "normal";
    const std::string kPosW = "posW";
    // others
    const std::string kShaderFile = "RenderPasses/RSMIndirectPass/RSMIndirectPass.slang";
    const std::string kColor = "IndirectColor";
}

RSMIndirectPass::SharedPtr RSMIndirectPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new RSMIndirectPass());
    return pPass;
}

Dictionary RSMIndirectPass::getScriptingDictionary()
{
    return Dictionary();
}

RSMIndirectPass::RSMIndirectPass()
    : RenderPass(kInfo)
{
   // mpState = GraphicsState::create();
    mpFbo = Fbo::create();
    mpPass = FullScreenPass::create(kShaderFile);

   // DepthStencilState::Desc dsDesc;
   // dsDesc.setDepthWriteMask(false).setDepthFunc(DepthStencilState::Func::LessEqual);
   // mpDsNoDepthWrite = DepthStencilState::create(dsDesc);
    srand(10086);
    //std::vector<uint32_t> data;
    //data.resize(64);

    //for (uint32_t i = 0; i < 64; i++)
    //{
    //    // Random directions on the XY plane
    //    float2 xi = (glm::linearRand(float2(-1), float2(1))) * 0.5f + 0.5f;
    //    float x = xi.x * sin(2 * M_PI * xi.y);
    //    float y = xi.x * cos(2 * M_PI * xi.y);
    //    data[i] = glm::packUnorm4x8(float4(x, y, xi.x, 0.0));
    //}

    //mpSamplesTex = Texture::create2D(64, 1, ResourceFormat::RGBA8Unorm, 1, Texture::kMaxPossible, data.data());
    float samples[512 * 4]{};
    for (int i = 0; i < 512; i++) {
        float2 xi = (glm::linearRand(float2(-1), float2(1))) * 0.5f + 0.5f;
        float x = xi.x * sin(2 * M_PI * xi.y);
        float y = xi.x * cos(2 * M_PI * xi.y);
        /*float xi1 = rand() / double(RAND_MAX);
        float xi2 = rand() / double(RAND_MAX);*/
        samples[4 * i] = x; 
        samples[4 * i + 1] = y;
        samples[4 * i + 2] = xi.x;
        samples[4 * i + 3] = 0; // 无效位
        // logInfo("SAMPLES: {},{},{}", x, y, xi1);
    }
    mpSamplesTex = Texture::create2D(512, 1, ResourceFormat::RGBA32Float, 1, Texture::kMaxPossible, samples);
}

RenderPassReflection RSMIndirectPass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    // reflector.addInput(kShadowDepth, "shadow map").format(ResourceFormat::D32Float).texture2D(0, 0);
    reflector.addInput(kShadowPosW, "RSM World space position").format(ResourceFormat::RGBA32Float).texture2D(mMapSize.x, mMapSize.y);
    reflector.addInput(kShadowNorm, "RSM World space normal").format(ResourceFormat::RGBA32Float).texture2D(mMapSize.x, mMapSize.y);
    reflector.addInput(kShadowColor, "Shadow Color").format(ResourceFormat::RGBA32Float).texture2D(mMapSize.x, mMapSize.y);
    // gbuffer
    reflector.addInput(kPosW, "GBuffer World space position").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    reflector.addInput(kNorm, "GBuffer World space normal").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    reflector.addOutput(kColor, "Indirect Illumination Color").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    return reflector;
}

void RSMIndirectPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;
   // mpVars = nullptr;

    if (mpScene)
    {
    }
}

void RSMIndirectPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene) return;
    //float samples[64 * 4]{};
    //for (int i = 0; i < 64; i++) {
    //    float2 xi = (glm::linearRand(float2(-1), float2(1))) * 0.5f + 0.5f;
    //    float x = xi.x * sin(2 * M_PI * xi.y);
    //    float y = xi.x * cos(2 * M_PI * xi.y);
    //    samples[4 * i + 2] = xi.x;
    //    samples[4 * i] = x;
    //    samples[4 * i + 1] = y;
    //    samples[4 * i + 3] = 0; // 无效位
    //}
    //mpSamplesTex = Texture::create2D(64, 1, ResourceFormat::RGBA32Float, 1, Texture::kMaxPossible, samples);
    // Texture::SharedPtr pShadowDepth = renderData.getTexture(kShadowDepth);
    Texture::SharedPtr pShadowNorm = renderData.getTexture(kShadowNorm);
    Texture::SharedPtr pShadowColor = renderData.getTexture(kShadowColor);
    Texture::SharedPtr pShadowPosW = renderData.getTexture(kShadowPosW);
    Texture::SharedPtr pNorm = renderData.getTexture(kNorm);
    Texture::SharedPtr pPosW = renderData.getTexture(kPosW);


    mpFbo->attachColorTarget(renderData.getTexture(kColor), 0);
    const float4 clearColor(0);
    pRenderContext->clearFbo(mpFbo.get(), clearColor, 1, 0, FboAttachmentType::All);


    InternalDictionary& dict = renderData.getDictionary();
    globalMat = dict["globalMat"];
    cascadeOffset = dict["cascadeOffset"];
    cascadeScale = dict["cascadeScale"];
    // for (int i = 0; i < 4; i++)
    //     logInfo("[RSMIndirectPass] globalMat: {}, {}, {}, {}", globalMat[i][0], globalMat[i][1], globalMat[i][2], globalMat[i][3]);
    /*globalMat = rmcv::mat4();
    globalMat[0] = rmcv::float4(-0.0011486155, 0, 0, -0.095504776);
    globalMat[1] = rmcv::float4(0, 0.00045544258, 0.0010544618, 0.5032824);
    globalMat[2] = rmcv::float4(0, -0.0005272309, 0.00022772129, 0.37690482);
    globalMat[3] = rmcv::float4(0, 0, 0, 1);
    cascadeScale = rmcv::float4(152.97307, 230.6321, 264.44342, 1);
    cascadeOffset = rmcv::float4(14.56938, -115.654655, -99.274925, 0);*/
    mpPass["PerFrameCB"]["globalMat"] = globalMat;
    mpPass["PerFrameCB"]["cascadeScale"] = cascadeScale;
    mpPass["PerFrameCB"]["cascadeOffset"] = cascadeOffset;
    mpPass["PerFrameCB"]["screenDimension"] = uint2(mpFbo->getWidth(), mpFbo->getHeight());
    mpPass["PerFrameCB"]["shadowMapDimension"] = uint2(2048, 2048);

    // logInfo("scale: ({}, {}), offset: ({}. {})",cascadeScale.x, cascadeScale.y, cascadeOffset.x, cascadeOffset.y);

    mpPass["gNormalTex"] = pNorm;
    mpPass["gWorldPosTex"] = pPosW;

    mpPass["rNormalTex"] = pShadowNorm;
    mpPass["rFluxTex"] = pShadowColor;
    mpPass["rWorldPosTex"] = pShadowPosW;
    // logInfo("shadow dimension: {}, {}", pShadowNorm->getWidth(), pShadowNorm->getHeight()); // 1920, 1080
    mpPass["samplesTex"] = mpSamplesTex;

    Sampler::Desc samplerDesc;
    // samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpPass["gSampler"] = Sampler::create(samplerDesc);
    mpPass->execute(pRenderContext, mpFbo);
}

void RSMIndirectPass::renderUI(Gui::Widgets& widget)
{
}
