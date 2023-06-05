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
#include "SSR.h"
#include "RenderGraph/RenderPassLibrary.h"

const RenderPass::Info SSR::kInfo { "SSR", "Insert pass description here." };


namespace
{
    const std::string kDirectIllumination = "directColor";
    const std::string kIndirectIllumination = "SSRcolorOriginal";
    const std::string kSpreadedIllumination = "SSRcolor";
    const std::string kPrevIndirectIllumination = "SSRcolorPrev";
    const std::string kDepth = "depth";
    const std::string kNormals = "normals";

    const std::string kShaderSSR = "RenderPasses/SSR/SSR.ps.slang";
    const std::string kShaderSpread = "RenderPasses/SSR/SSR_spread.ps.slang";


    const std::string kPrevColor = "prevColor";
}

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(SSR::kInfo, SSR::create);
}

SSR::SSR()
    : RenderPass(kInfo)
{
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    mpTextureSampler = Sampler::create(samplerDesc);

    mpSSRPass = FullScreenPass::create(kShaderSSR);
    mpSpreadPass = FullScreenPass::create(kShaderSpread);

    Fbo::Desc fboDesc;
    mpFbo = Fbo::create();
    mpFboSpread = Fbo::create();
    mCurFrameCnt = 0;
}


SSR::SharedPtr SSR::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new SSR());
    return pPass;
}

Dictionary SSR::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection SSR::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addOutput(kSpreadedIllumination, "Color-buffer with SSR spreading applied to it").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    reflector.addInternal(kIndirectIllumination, "Color-buffer with SSR applied to it").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    // reflector.addInternal(kPrevIndirectIllumination, "Previous color-buffer with SSR applied to it").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    reflector.addInput(kNormals, "World space normals, [0, 1] range").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    reflector.addInput(kDepth, "Depth-buffer").texture2D(0, 0);
    reflector.addInput(kDirectIllumination, "Direct Illumination Color buffer").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    return reflector;
}

void SSR::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene) return;
    addDefines();

    auto pDepth = renderData.getTexture(kDepth);
    auto pNormals = renderData.getTexture(kNormals);
    auto pColorSSR = renderData.getTexture(kIndirectIllumination);
    auto pSSRColorSpreaded = renderData.getTexture(kSpreadedIllumination);
    auto pColorIn = renderData.getTexture(kDirectIllumination);   

    FALCOR_ASSERT(pColorSSR != pColorIn);

    mpFbo->attachColorTarget(pColorSSR, 0);
    mpFboSpread->attachColorTarget(pSSRColorSpreaded, 0);
    const float4 clearColor(0);
    pRenderContext->clearFbo(mpFbo.get(), clearColor, 1, 0, FboAttachmentType::All);
    if(mOptionsDirty)
        pRenderContext->clearFbo(mpFboSpread.get(), clearColor, 1, 0, FboAttachmentType::All);
    
    auto pCamera = mpScene->getCamera().get();
    
    ShaderVar var = mpSSRPass["PerFrameCB"];
    pCamera->setShaderData(var["gCamera"]);
    var["screenDimension"] = uint2(mpFbo->getWidth(), mpFbo->getHeight());
    var["frameCnt"] = uint(mCurFrameCnt);
    var["hysteresis"] = mOptionsSSR.hysteresis;
    var["prevInvViewProj"] = prevInvViewProj;
    // mpSSDOPass["gNoiseSampler"] = mpNoiseSampler;
    mpSSRPass["gTextureSampler"] = mpTextureSampler;
    mpSSRPass["gDepthTex"] = pDepth;
    // mpSSDOPass["gNoiseTex"] = mpNoiseTexture;
    mpSSRPass["gNormalTex"] = pNormals;
    mpSSRPass["gColorTex"] = pColorIn;
    mpSSRPass["gPrevColorTex"] = pSSRColorSpreaded;

    mpSSRPass->execute(pRenderContext, mpFbo);
    
    mpSpreadPass["gColorTexSSR"] = pColorSSR;
    mpSpreadPass["gNormalTex"] = pNormals;
    mpSpreadPass["PerFrameCB"]["frameCnt"] = uint(mCurFrameCnt);
    mpSpreadPass["PerFrameCB"]["screenDimension"] = uint2(mpFbo->getWidth(), mpFbo->getHeight());

    pRenderContext->clearFbo(mpFboSpread.get(), clearColor, 1, 0, FboAttachmentType::All);
    mpSpreadPass->execute(pRenderContext, mpFboSpread);
    mCurFrameCnt++;
    prevInvViewProj = pCamera->getViewProjMatrix();
}

void SSR::renderUI(Gui::Widgets& widget)
{
    
    mOptionsDirty |= widget.checkbox("Debug Ray", mOptionsSSR.visualizeRay);
    mOptionsDirty |= widget.checkbox("Reflect", mOptionsSSR.reflect);
    widget.var("zThickness", mOptionsSSR.zThickness, 0.f, 1.f);
    if (!mOptionsSSR.reflect && !mOptionsSSR.visualizeRay) {
        widget.checkbox("Recursive Radiance", mOptionsSSR.timeInterpolation);
        if (mOptionsSSR.timeInterpolation) {
            widget.var("hysteresis", mOptionsSSR.hysteresis, 0.f, 1.f);
        }
        widget.var("Num rays per pixel", mOptionsSSR.sampleNum, 1, 3);
        widget.var("Radiance Spread Radius (pixel)", mOptionsSSR.spreadRadius);
        widget.var("Num pixels to spread", mOptionsSSR.spreadSampleNum, 0);
    }
    else {
        mOptionsSSR.spreadSampleNum = 0;
        mOptionsSSR.spreadRadius = 0;
    }
}

void SSR::addDefines() {
    mpSSRPass->addDefine("SAMPLE_NUM", std::to_string(mOptionsSSR.sampleNum));
    mpSSRPass->addDefine("VISUALIZE_RAY", std::to_string(mOptionsSSR.visualizeRay ? 1 : 0));
    mpSSRPass->addDefine("REFLECT", std::to_string(mOptionsSSR.reflect ? 1 : 0));
    mpSSRPass->addDefine("Z_THICKNESS", std::to_string(mOptionsSSR.zThickness));
    mpSSRPass->addDefine("TIME_INTERPOLATION", std::to_string(mOptionsSSR.timeInterpolation ? 1 : 0));

    mpSpreadPass->addDefine("SPREAD_RADIUS", std::to_string(mOptionsSSR.spreadRadius));
    mpSpreadPass->addDefine("SAMPLE_NUM", std::to_string(mOptionsSSR.spreadSampleNum));
}
