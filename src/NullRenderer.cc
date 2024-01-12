// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "NullRenderer.h"

namespace hdMoonray {

NullRenderer::NullRenderer(scene_rdl2::rdl2::SceneContext* context)
{
    mSceneContext = context;
}

NullRenderer::~NullRenderer() {}

bool
NullRenderer::allocate(scene_rdl2::rdl2::RenderOutput*, PixelData& pd, const PixelSize& requested)
{
    return false;
}

bool
NullRenderer::resolve(scene_rdl2::rdl2::RenderOutput*, PixelData& pd)
{
    return false;
}

void
NullRenderer::deallocate(scene_rdl2::rdl2::RenderOutput*, PixelData&)
{ }

void
NullRenderer::applySettings(const RenderSettings&)
{ }

float
NullRenderer::getProgress() const
{
    return 1.0f;
}

float
NullRenderer::getElapsedSeconds() const
{
    return 0.0f;
}

// warning: multithreaded calls
void
NullRenderer::beginUpdate()
{
    mUpdateActive = true;
}

void
NullRenderer::endUpdate()
{
    mUpdateActive = false;
}

bool
NullRenderer::isFrameComplete() const
{
    return true;
}

}

