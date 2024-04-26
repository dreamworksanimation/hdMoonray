// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Geometry.h"

#include <pxr/imaging/hd/points.h>

namespace hdMoonray {

class Points final : public pxr::HdPoints, public Geometry
{
public:
    explicit Points(pxr::SdfPath const& id INSTANCERID(const pxr::SdfPath&));
    const std::string& className(pxr::HdSceneDelegate* sceneDelegate) const override;

    /// Dirty bits to pass to first call to Sync()
    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    /// Update the data identified by dirtyBits. Must not query other data.
    void Sync(pxr::HdSceneDelegate* sceneDelegate,
              pxr::HdRenderParam*   renderParam,
              pxr::HdDirtyBits*     dirtyBits,
              const pxr::TfToken&   reprToken) override;

    void Finalize(pxr::HdRenderParam* renderParam) override;

protected:
    // Initialize one of the reprs, called before Sync()
    void _InitRepr(pxr::TfToken const &reprToken, pxr::HdDirtyBits *dirtyBits) override {}

    // Expand dirty bits to what Sync() actually needs
    pxr::HdDirtyBits _PropagateDirtyBits(pxr::HdDirtyBits bits) const override { return bits; }

private:
    // This class does not support copying.
    Points(const Points&)             = delete;
    Points &operator =(const Points&) = delete;
};

}

