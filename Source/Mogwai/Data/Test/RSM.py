from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    loadRenderPassLibrary('AccumulatePass.dll')
    loadRenderPassLibrary('BSDFViewer.dll')
    loadRenderPassLibrary('BlendPass.dll')
    loadRenderPassLibrary('Antialiasing.dll')
    loadRenderPassLibrary('BlitPass.dll')
    loadRenderPassLibrary('CSM.dll')
    loadRenderPassLibrary('DebugPasses.dll')
    loadRenderPassLibrary('PathTracer.dll')
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('DLSSPass.dll')
    loadRenderPassLibrary('ErrorMeasurePass.dll')
    loadRenderPassLibrary('SimplePostFX.dll')
    loadRenderPassLibrary('ShadowDepthPass.dll')
    loadRenderPassLibrary('FLIPPass.dll')
    loadRenderPassLibrary('ForwardLightingPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('WhittedRayTracer.dll')
    loadRenderPassLibrary('ImageLoader.dll')
    loadRenderPassLibrary('MinimalPathTracer.dll')
    loadRenderPassLibrary('ModulateIllumination.dll')
    loadRenderPassLibrary('myRenderLibrary.dll')
    loadRenderPassLibrary('NRDPass.dll')
    loadRenderPassLibrary('PixelInspectorPass.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('RTXDIPass.dll')
    loadRenderPassLibrary('RSMBuffer.dll')
    loadRenderPassLibrary('RSMIndirectPass.dll')
    loadRenderPassLibrary('RTXGIPass.dll')
    loadRenderPassLibrary('SceneDebugger.dll')
    loadRenderPassLibrary('SDFEditor.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('WireframePass.dll')
    loadRenderPassLibrary('SVGFPass.dll')
    loadRenderPassLibrary('TemporalDelayPass.dll')
    loadRenderPassLibrary('TestPasses.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('Utils.dll')
    loadRenderPassLibrary('VisibilityPass.dll')
    RSMBuffer = createPass('RSMBuffer')
    g.addPass(RSMBuffer, 'RSMBuffer')
    DepthPass = createPass('DepthPass', {'depthFormat': ResourceFormat.D32Float, 'useAlphaTest': True})
    g.addPass(DepthPass, 'DepthPass')
    SkyBox = createPass('SkyBox', {'texName': WindowsPath('.'), 'loadAsSrgb': True, 'filter': SamplerFilter.Linear})
    g.addPass(SkyBox, 'SkyBox')
    ForwardLightingPass = createPass('ForwardLightingPass', {'sampleCount': 1, 'enableSuperSampling': False})
    g.addPass(ForwardLightingPass, 'ForwardLightingPass')
    CSM = createPass('CSM', {'mapSize': uint2(2048,2048), 'visibilityBufferSize': uint2(0,0), 'cascadeCount': 1, 'visibilityMapBitsPerChannel': 32, 'kSdsmReadbackLatency': 1, 'blurWidth': 5, 'blurSigma': 2.0})
    g.addPass(CSM, 'CSM')
    RSMIndirectPass = createPass('RSMIndirectPass')
    g.addPass(RSMIndirectPass, 'RSMIndirectPass')
    BlendPass = createPass('BlendPass')
    g.addPass(BlendPass, 'BlendPass')
    g.addEdge('DepthPass.depth', 'SkyBox.depth')
    g.addEdge('ForwardLightingPass.posW', 'RSMIndirectPass.posW')
    g.addEdge('SkyBox.target', 'ForwardLightingPass.color')
    g.addEdge('DepthPass.depth', 'ForwardLightingPass.depth')
    g.addEdge('DepthPass.depth', 'CSM.depth')
    g.addEdge('CSM.visibility', 'ForwardLightingPass.visibilityBuffer')
    g.addEdge('ForwardLightingPass.normals', 'RSMIndirectPass.normal')
    g.addEdge('RSMBuffer.worldPosition', 'RSMIndirectPass.shadowWorldPosition')
    g.addEdge('RSMBuffer.normal', 'RSMIndirectPass.shadowNormal')
    g.addEdge('RSMBuffer.color', 'RSMIndirectPass.shadowColor')
    g.addEdge('RSMIndirectPass.IndirectColor', 'BlendPass.texSrc1')
    g.addEdge('ForwardLightingPass.color', 'BlendPass.texSrc2')
    g.addEdge('CSM.shadowMap', 'RSMBuffer.shadowDepth')
    g.markOutput('RSMIndirectPass.IndirectColor')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
