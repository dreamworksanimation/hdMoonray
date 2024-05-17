// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "NullRenderer.h"

namespace hdMoonray {

NullRenderer::NullRenderer()
{
    mSceneContext = new scene_rdl2::rdl2::SceneContext();
}

NullRenderer::~NullRenderer()
{
    delete mSceneContext;
}

bool 
NullRenderer::allocate(scene_rdl2::rdl2::RenderOutput* ro, PixelData& pd, const PixelSize& request) 
{
    // we have to allocate a buffer, even though we won't fill it,
    // because the calling app may fail without one
    // (we could perhaps add an option to disable this)
    pd.mChannels = isBeauty(ro) ? 4 : request.mChannels;
    pd.mWidth = request.mWidth;
    pd.mHeight = request.mHeight;
    pd.vec.resize(pd.mWidth * pd.mHeight * pd.mChannels);
    pd.mData = pd.vec.data();

    return true;
}

bool
NullRenderer::resolve(scene_rdl2::rdl2::RenderOutput*, PixelData&) 
{
    return false;
}
    
void 
NullRenderer::deallocate(scene_rdl2::rdl2::RenderOutput*, PixelData&) 
{
}

}
