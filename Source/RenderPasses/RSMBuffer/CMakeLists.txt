add_renderpass(RSMBuffer)

target_sources(RSMBuffer PRIVATE
    RSMBuffer.cpp
    RSMBuffer.h
    ShadowPass.slang
)

target_copy_shaders(CSM RenderPasses/RSMBuffer)

target_source_group(RSMBuffer "RenderPasses")
