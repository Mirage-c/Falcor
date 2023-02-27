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
#include "WireframePass.h"
#include "RenderGraph/RenderPassLibrary.h"

const RenderPass::Info WireframePass::kInfo { "WireframePass", "Renders a scene as a wireframe." };

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(WireframePass::kInfo, WireframePass::create);
}

WireframePass::SharedPtr WireframePass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new WireframePass());
    return pPass;
}

Dictionary WireframePass::getScriptingDictionary()
{
    return Dictionary();
}

void WireframePass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;
    if (mpScene) mpProgram->addDefines(mpScene->getSceneDefines());
    mpVars = GraphicsVars::create(mpProgram->getReflector());
}

RenderPassReflection WireframePass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addOutput("dst", "Renders a scene as a wireframe");
    return reflector;
}

void WireframePass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // creating and binding the fbo
    auto pTargetFbo = Fbo::create({ renderData.getTexture("output") });
    const float4 clearColor(0, 0, 0, 1);
    pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All); // remove any data from previous executions
    mpGraphicsState->setFbo(pTargetFbo); // bind the fbo

    if (mpScene)
    {
        // Set render state
        // Falcor::RasterizerState::CullMode renderFlags = Falcor::RasterizerState::;
        mpVars["PerFrameCB"]["gColor"] = float4(0, 1, 0, 1);

        mpScene->rasterize(pRenderContext, mpGraphicsState.get(), mpVars.get());
    }
}

void WireframePass::renderUI(Gui::Widgets& widget)
{
}
