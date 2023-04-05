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
#include "SSDO.h"
#include "RenderGraph/RenderPassLibrary.h"
#include "Utils/Math/FalcorMath.h"
#include <glm/gtc/random.hpp>

const RenderPass::Info SSDO::kInfo { "SSDO", "Insert pass description here." };

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(SSDO::kInfo, SSDO::create);
}

namespace
{
    const std::string kAoMapSize = "aoMapSize";
    const std::string kKernelSize = "kernelSize";
    const std::string kNoiseSize = "noiseSize";
    const std::string kDistribution = "distribution";
    const std::string kRadius = "radius";
    const std::string kBlurKernelWidth = "blurWidth";
    const std::string kBlurSigma = "blurSigma";

    const std::string kColorIn = "colorIn";
    const std::string kColorOut = "colorOut";
    const std::string kDepth = "depth";
    const std::string kNormals = "normals";
    const std::string kAoMap = "AoMap";

    const std::string kSSDOShader = "RenderPasses/SSDO/SSDO.ps.slang";
}

SSDO::SSDO()
    : RenderPass(kInfo)
{
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point).setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap);
    mpNoiseSampler = Sampler::create(samplerDesc);

    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    mpTextureSampler = Sampler::create(samplerDesc);

    mpSSDOPass = FullScreenPass::create(kSSDOShader);
}

SSDO::SharedPtr SSDO::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new SSDO());
    return pPass;
}

Dictionary SSDO::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection SSDO::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addOutput(kColorOut, "Color-buffer with AO applied to it").texture2D(0, 0);
    reflector.addInput(kNormals, "World space normals, [0, 1] range").texture2D(0, 0);
    reflector.addInput(kDepth, "Depth-buffer").texture2D(0, 0);
    reflector.addInput(kColorIn, "Color buffer").texture2D(0, 0);
    return reflector;
}

void SSDO::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene) return;
    // Run the AO pass
    auto pDepth = renderData.getTexture(kDepth);
    auto pNormals = renderData.getTexture(kNormals);
    auto pColorOut = renderData.getTexture(kColorOut);
    auto pColorIn = renderData.getTexture(kColorIn);
    auto pAoMap = renderData.getTexture(kAoMap);

    FALCOR_ASSERT(pColorOut != pColorIn);
    mpDOFbo->attachColorTarget(pColorOut, 0);
    auto pCamera = mpScene->getCamera().get();
    if (mDirty)
    {
        ShaderVar var = mpSSDOPass["StaticCB"];
        if (var.isValid()) var.setBlob(mData);
        mDirty = false;
    }
    ShaderVar var = mpSSDOPass["PerFrameCB"];
    pCamera->setShaderData(var["gCamera"]);
    // mpSSDOPass["PerFrameCB"]["screenDimension"] = uint2(mpDOFbo->getWidth(), mpDOFbo->getHeight());
    // logInfo("screen Dim : {}, {}", mpDOFbo->getWidth(), mpDOFbo->getHeight());
    mpSSDOPass["gNoiseSampler"] = mpNoiseSampler;
    mpSSDOPass["gTextureSampler"] = mpTextureSampler;
    mpSSDOPass["gDepthTex"] = pDepth;
    mpSSDOPass["gNoiseTex"] = mpNoiseTexture;
    mpSSDOPass["gNormalTex"] = pNormals;
    mpSSDOPass["gColorTex"] = pColorIn;

    mpSSDOPass->execute(pRenderContext, mpDOFbo);

}

void SSDO::renderUI(Gui::Widgets& widget)
{
}

void SSDO::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    Fbo::Desc fboDesc;
    fboDesc.setColorTarget(0, Falcor::ResourceFormat::R8Unorm);
    mpDOFbo = Fbo::create2D(mDoMapSize.x, mDoMapSize.y, fboDesc);

    setKernel();
    setNoiseTexture(mNoiseSize.x, mNoiseSize.y);

    /*mpBlurGraph = RenderGraph::create("Gaussian Blur");
    GaussianBlur::SharedPtr pBlurPass = GaussianBlur::create(pRenderContext, mBlurDict);
    mpBlurGraph->addPass(pBlurPass, "GaussianBlur");
    mpBlurGraph->markOutput("GaussianBlur.dst");*/
}

void SSDO::setKernel()
{
    for (uint32_t i = 0; i < mData.kernelSize; i++)
    {
        float3 p;
        p = hammersleyCosine(i, mData.kernelSize);

        mData.sampleKernel[i] = float4(p, 0.0f);

        // Skew sample point distance on a curve so more cluster around the origin
        float dist = (float)i / (float)mData.kernelSize;
        dist = glm::mix(0.1f, 1.0f, dist * dist);
        mData.sampleKernel[i] *= dist;
    }
    mDirty = true;
}

void SSDO::setNoiseTexture(uint32_t width, uint32_t height)
{
    std::vector<uint32_t> data;
    data.resize(width * height);

    for (uint32_t i = 0; i < width * height; i++)
    {
        // Random directions on the XY plane
        float2 dir = glm::normalize(glm::linearRand(float2(-1), float2(1))) * 0.5f + 0.5f;
        data[i] = glm::packUnorm4x8(float4(dir, 0.0f, 1.0f));
    }

    mpNoiseTexture = Texture::create2D(width, height, ResourceFormat::RGBA8Unorm, 1, Texture::kMaxPossible, data.data());

    mData.noiseScale = float2(mpDOFbo->getWidth(), mpDOFbo->getHeight()) / float2(width, height);

    mDirty = true;
}
