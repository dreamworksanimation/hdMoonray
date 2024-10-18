// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "GeometryMixin.h"
#include <pxr/imaging/hd/points.h>

namespace hdMoonray {

class Points final : public pxr::HdPoints, public GeometryMixin
{
public:
    explicit Points(pxr::SdfPath const& id);

    /// Dirty bits to pass to first call to Sync()
    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    /// Update the data identified by dirtyBits. Must not query other data.
    void Sync(pxr::HdSceneDelegate* sceneDelegate,
              pxr::HdRenderParam*   renderParam,
              pxr::HdDirtyBits*     dirtyBits,
              const pxr::TfToken&   reprToken) override;

    void Finalize(pxr::HdRenderParam* renderParam) override;

protected:

    // Hydra overrides
    void _InitRepr(pxr::TfToken const &reprToken, pxr::HdDirtyBits *dirtyBits) override {}
    pxr::HdDirtyBits _PropagateDirtyBits(pxr::HdDirtyBits bits) const override { return bits; }

    // GeometryMixin overrides
    void primvarChanged(pxr::HdSceneDelegate *sceneDelegate, RenderDelegate& renderDelegate,
                        const pxr::TfToken& name,  const pxr::VtValue& value,
                        const pxr::HdInterpolation& interp, const pxr::TfToken& role);
private:
    Points(const Points&)             = delete;
    Points &operator =(const Points&) = delete;
};

}

