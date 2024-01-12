// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file MapGuard.h

#pragma once

#include <pxr/imaging/hd/renderBuffer.h>

namespace hd_render {

// RAII proctection for render buffer mapping/unmapping
// Upon construction, the render buffer is mapped and pixels
// are made available.  Upon deletion, the render buffer is
// unmapped.
class MapGuard
{
public:
    MapGuard(pxr::HdRenderBuffer *renderBuffer):
        mRenderBuffer(renderBuffer)
    {
        mPixels = mRenderBuffer->Map();
    }

    ~MapGuard()
    {
        mRenderBuffer->Unmap();
    }

    MapGuard(const MapGuard &) = delete;
    MapGuard &operator=(const MapGuard &) = delete;

    void *getPixels() const { return mPixels; }

private:
    void *mPixels;
    pxr::HdRenderBuffer *mRenderBuffer;
};

} // namespace hd_render

