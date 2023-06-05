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
#include "BlendPass.h"
#include "RenderGraph/RenderPassLibrary.h"

const RenderPass::Info BlendPass::kInfo { "BlendPass", "Insert pass description here." };

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(BlendPass::kInfo, BlendPass::create);
}

namespace {
    const std::string kTex1 = "texSrc1";
    const std::string kTex2 = "texSrc2";
    // others
    const std::string kShaderFile = "RenderPasses/BlendPass/BlendPass.slang";
    const std::string kDst = "texDst";

    const std::string kPrevColor = "prevColor";
}

BlendPass::BlendPass()
    : RenderPass(kInfo)
{
    // mpState = GraphicsState::create();
    mpFbo = Fbo::create();
    mpPass = FullScreenPass::create(kShaderFile);
}

BlendPass::SharedPtr BlendPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new BlendPass());
    return pPass;
}

Dictionary BlendPass::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection BlendPass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addInput(kTex1, "input texture 1").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    reflector.addInput(kTex2, "input texture 2").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    reflector.addOutput(kDst, "output texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0);
    return reflector;
}

void BlendPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;
    // mpVars = nullptr;

    if (mpScene)
    {
    }
}

void BlendPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{

    if (!mpScene) return;
    // renderData holds the requested resources
    auto& pTexture1 = renderData.getTexture(kTex1);
    auto& pTexture2 = renderData.getTexture(kTex2);
    auto& pColor = renderData.getTexture(kDst);

    mpFbo->attachColorTarget(pColor, 0);
    const float4 clearColor(0);
    pRenderContext->clearFbo(mpFbo.get(), clearColor, 1, 0, FboAttachmentType::All);

    mpPass["srcTex1"] = pTexture1;
    mpPass["srcTex2"] = pTexture2;
    mpPass["PerFrameCB"]["screenDimension"] = uint2(mpFbo->getWidth(), mpFbo->getHeight());

    mpPass->execute(pRenderContext, mpFbo);

    // InternalDictionary& dict = renderData.getDictionary();
    // pRenderContext->copyResource(prevColor.get(), pColor.get());
    // dict[kPrevColor] = prevColor;
}

void BlendPass::renderUI(Gui::Widgets& widget)
{
}
