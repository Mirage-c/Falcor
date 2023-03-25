add_renderpass(VisibilityPass)

target_sources(VisibilityPass PRIVATE
    VisibilityPass.cpp
    VisibilityPass.h
)

target_source_group(VisibilityPass "RenderPasses")
