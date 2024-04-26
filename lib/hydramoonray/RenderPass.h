// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/imaging/hd/renderPass.h>

namespace hdMoonray {

class RenderDelegate;
class Camera;

/// RenderPass represents a single render iteration, rendering a view of the
/// scene (the HdRprimCollection) for a specific viewer (the camera/viewport
/// parameters in HdRenderPassState) to the current draw target.

class RenderPass final: public pxr::HdRenderPass
{
public:
    // constructor. Directly reads data such as the RenderContext from RenderDelegate
    RenderPass(
        pxr::HdRenderIndex* index,
        const pxr::HdRprimCollection& collection,
        RenderDelegate* d
    ) : HdRenderPass(index, collection), renderDelegate(*d) { }

    ~RenderPass();

    bool IsConverged() const override;

protected:
    void _Execute(pxr::HdRenderPassStateSharedPtr const& renderPassState,
                  pxr::TfTokenVector const &renderTags) override;

    void _MarkCollectionDirty() override;
    void _Sync() override;

private:
    RenderDelegate& renderDelegate; // RenderDelegate that called constructor
    mutable bool mDeferIsConverged = false;
};

}

