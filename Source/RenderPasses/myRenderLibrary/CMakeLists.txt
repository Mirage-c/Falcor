add_renderpass(myRenderLibrary)

target_sources(myRenderLibrary PRIVATE
    myRenderLibrary.cpp
    myRenderLibrary.h
)

target_source_group(myRenderLibrary "RenderPasses")
