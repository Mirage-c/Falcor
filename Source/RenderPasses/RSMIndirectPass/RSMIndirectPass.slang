import Scene.Raster;
import Utils.Math.MathHelpers;
import Rendering.Lights.LightHelpers;
import Rendering.Lights.EnvMapLighting;
import Utils.Sampling.TinyUniformSampleGenerator;


cbuffer PerFrameCB
{
};

Texture2D samplesTex;

VSOut vsMain(VSIn vIn)
{
    VSOut vsOut;
    vsOut = defaultVS(vIn);
    return vsOut;
}

struct PsOut
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
#ifdef _OUTPUT_MOTION_VECTORS
    float2 motion : SV_TARGET2;
#endif
};

PsOut psMain(VSOut vsOut, uint triangleIndex: SV_PrimitiveID)
{
}