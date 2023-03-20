add_renderpass(ShadowDepthPass)

target_sources(ShadowDepthPass PRIVATE
    ShadowDepthPass.cpp
    ShadowDepthPass.h
)

target_source_group(ShadowDepthPass "RenderPasses")
