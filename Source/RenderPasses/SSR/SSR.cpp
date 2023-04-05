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
    const std::string kIndirectIllumination = "SSRcolor";
    const std::string kDepth = "depth";
    const std::string kNormals = "normals";

    const std::string kShader = "RenderPasses/SSR/SSR.ps.slang";
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

    mpPass = FullScreenPass::create(kShader);

    Fbo::Desc fboDesc;
    mpFbo = Fbo::create();
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
    reflector.addOutput(kIndirectIllumination, "Color-buffer with SSR applied to it").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    reflector.addInput(kNormals, "World space normals, [0, 1] range").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    reflector.addInput(kDepth, "Depth-buffer").texture2D(0, 0);
    reflector.addInput(kDirectIllumination, "Direct Illumination Color buffer").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    return reflector;
}

void SSR::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene) return;
    // Run the AO pass
    auto pDepth = renderData.getTexture(kDepth);
    auto pNormals = renderData.getTexture(kNormals);
    auto pColorOut = renderData.getTexture(kIndirectIllumination);
    auto pColorIn = renderData.getTexture(kDirectIllumination);

    FALCOR_ASSERT(pColorOut != pColorIn);
    mpFbo->attachColorTarget(pColorOut, 0);
    const float4 clearColor(0);
    pRenderContext->clearFbo(mpFbo.get(), clearColor, 1, 0, FboAttachmentType::All);

    auto pCamera = mpScene->getCamera().get();
    /*if (mDirty)
    {
        ShaderVar var = mpSSDOPass["StaticCB"];
        if (var.isValid()) var.setBlob(mData);
        mDirty = false;
    }*/
    ShaderVar var = mpPass["PerFrameCB"];
    pCamera->setShaderData(var["gCamera"]);
    mpPass["PerFrameCB"]["screenDimension"] = uint2(mpFbo->getWidth(), mpFbo->getHeight());
    // mpSSDOPass["gNoiseSampler"] = mpNoiseSampler;
    mpPass["gTextureSampler"] = mpTextureSampler;
    mpPass["gDepthTex"] = pDepth;
    // mpSSDOPass["gNoiseTex"] = mpNoiseTexture;
    mpPass["gNormalTex"] = pNormals;
    mpPass["gColorTex"] = pColorIn;

    mpPass->execute(pRenderContext, mpFbo);
}

void SSR::renderUI(Gui::Widgets& widget)
{
}
