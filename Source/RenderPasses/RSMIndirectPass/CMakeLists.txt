add_renderpass(RSMIndirectPass)

target_sources(RSMIndirectPass PRIVATE
    RSMIndirectPass.cpp
    RSMIndirectPass.h
)

target_source_group(RSMIndirectPass "RenderPasses")
