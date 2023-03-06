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
import RenderPasses.CSM.CascadedShadowMap;
import CascadedShadowMap;

layout(binding = 0) SamplerState alphaSampler : register(s0);
layout(binding = 1) texture2D alphaMap : register(t0);

layout(binding = 2) cbuffer AlphaMapCB : register(b1)
{
    float alphaThreshold;
};

layout(binding = 3) cbuffer PerLightCB : register(b0)
{
    CsmData gCsmData;
};

struct ShadowPassPSIn
{
    float2 texC : TexCoord;
    float4 pos : SV_POSITION;
    uint rtIndex : SV_RenderTargetArrayIndex;
};

struct ShadowPassVSOut
{
    float2 texC : TEXCOORD;
    float4 pos : POSITION;
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
    // const GeometryInstanceID instanceID = { vIn.instanceID };

    // float4x4 worldMat = gScene.getWorldMatrix(instanceID);
    // vOut.pos = mul(worldMat, float4(vIn.pos, 1.f));
    vOut.posH = mul(gCsmData.globalMat, float4(vOut.posW, 1.f));
    vOut.posH.xyz *= gCsmData.cascadeScale[0].xyz;
    vOut.posH.xyz += gCsmData.cascadeOffset[0].xyz;
    // vOut.texC = vIn.texC;
    return vOut;
}

float4 psMain(VSOut vsout) : SV_TARGET0
{

    PsOut psout;
    psout.color = float4(1.f, 0.f, 0.f, 1.f);
    psout.normW = float4(0.f, 1.f, 1.f, 1.f);
    psout.posW = float4(1.f, 1.f, 1.f, 1.f);
    return float4(1.f, 1.f, 1.f, 1.f);
}

