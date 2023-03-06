import Scene.Raster; // Imports defaultVS.

cbuffer PerFrameCB
{
    float4 gColor;
    float4x4 globalMat;
};

VSOut vsMain(VSIn vIn)
{
    VSOut vOut;
    vOut = defaultVS(vIn);
    // return vOut;
    vOut.posH = mul(globalMat, float4(vOut.posW, 1.f));
    return vOut;
}

[maxvertexcount(3)]
void gsMain(triangle VSOut input[3], uint InstanceID : SV_GSInstanceID, inout TriangleStream<VSOut> outStream)
{
    ShadowPassPSIn outputData;

    for(int i = 0 ; i < 3 ; i++)
    {
        outputData.pos = mul(gCsmData.globalMat, input[i].pos);
        outputData.pos.xyz /= input[i].pos.w;
        outputData.pos.xyz *= gCsmData.cascadeScale[InstanceID].xyz;
        outputData.pos.xyz += gCsmData.cascadeOffset[InstanceID].xyz;

        outputData.texC = input[i].texC;
        outputData.rtIndex = InstanceID;

        outStream.Append(outputData);
    }

    outStream.RestartStrip();
}

float4 psMain() : SV_TARGET
{
    return gColor;
}