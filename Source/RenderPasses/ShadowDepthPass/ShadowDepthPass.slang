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


VSOut vsMain(VSIn vIn)
{
    VSOut vOut;
    vOut = defaultVS(vIn);
    vOut.posH = mul(globalMat, float4(vOut.posW, 1.f));
    vOut.posH.xyz *= cascadeScale.xyz;
    vOut.posH.xyz += cascadeOffset.xyz;
    return vOut;
}

void psMain(VSOut pIn, uint triangleIndex: SV_PrimitiveID) : SV_TARGET
{
}
