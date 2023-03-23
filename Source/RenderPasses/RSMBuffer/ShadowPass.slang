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
#include "Scene/VertexAttrib.slangh"
import Scene.Raster;
import Utils.Math.MathHelpers;
import Rendering.Lights.LightHelpers;
import Rendering.Lights.EnvMapLighting;
import Utils.Sampling.TinyUniformSampleGenerator;

cbuffer PerFrameCB
{
    float4x4 globalMat;
    float4 cascadeScale;
    float4 cascadeOffset;
};

struct PsOut
{
    // float4 depth : SV_TARGET0;
    float4 posW : SV_TARGET0;
    float4 normW : SV_TARGET1;
    float4 color : SV_TARGET2;
};

VSOut vsMain(VSIn vIn)
{
    VSOut vOut;
    vOut = defaultVS(vIn);
    vOut.posH = mul(globalMat, float4(vOut.posW, 1.f));
    vOut.posH.xyz *= cascadeScale.xyz;
    vOut.posH.xyz += cascadeOffset.xyz;
    return vOut;
}


PsOut psMain(VSOut pIn, uint triangleIndex : SV_PrimitiveID)
{
    PsOut psout;
    // psout.depth = float4(float3(pIn.posH.z), 1.f);
    psout.posW = float4(float3(pIn.posW), 1.f);

    let lod = ImplicitLodTextureSampler();
    // if (alphaTest(pIn, triangleIndex, lod)) discard;

    float3 viewDir = normalize(gScene.camera.getPosition() - pIn.posW);
    
    ShadingData sd = prepareShadingData(pIn, triangleIndex, viewDir, lod);
    psout.normW = float4(sd.N * 0.5f + 0.5f, 1.0f);

    AnalyticLightSample ls;
    evalLightApproximate(sd.posW, gScene.getLight(0), ls);
    let bsdf = gScene.materials.getBSDF(sd, lod);
    let bsdfProperties = bsdf.getProperties(sd);

    // let material = gScene.materials.getMaterial(sd.materialID);

    // const uint2 pixel = pIn.posH.xy;
    // TinyUniformSampleGenerator sg = TinyUniformSampleGenerator(pixel, 0);
    // psout.color.rgb = bsdf.eval(sd, ls.dir, sg) * ls.Li;
    // if (any(bsdfProperties.diffuse > float3(0.f, 0.f, 0.f)))
    psout.color.rgb = bsdfProperties.diffuse * ls.Li; // or diffuse transmission?
    // psout.color = float4(1.f, 0.f, 0.f, 1.f);
    // psout.normW = float4(0.f, 1.f, 1.f, 1.f);
    // psout.posW = float4(1.f, 1.f, 1.f, 1.f);
    return psout;
}
