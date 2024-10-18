// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "GeometryMixin.h"
#include <pxr/imaging/hd/basisCurves.h>

namespace hdMoonray {

class BasisCurves final : public pxr::HdBasisCurves, public GeometryMixin
{
public:
    explicit BasisCurves(pxr::SdfPath const& id);

    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

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
    void syncAttributes(pxr::HdSceneDelegate* sceneDelegate, RenderDelegate& renderDelegate,
                        pxr::HdDirtyBits* dirtyBits, const pxr::TfToken& reprToken) override;
    void primvarChanged(pxr::HdSceneDelegate *sceneDelegate, RenderDelegate& renderDelegate,
                        const pxr::TfToken& name,  const pxr::VtValue& value,
                        const pxr::HdInterpolation& interp, const pxr::TfToken& role) override;
private:
    BasisCurves(const BasisCurves&)             = delete;
    BasisCurves &operator =(const BasisCurves&) = delete;

    void syncTopology(const pxr::HdBasisCurvesTopology& topology);
    void syncDisplayStyle(const pxr::HdDisplayStyle& style);
};

}

