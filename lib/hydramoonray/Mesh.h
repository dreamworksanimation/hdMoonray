// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Geometry.h"

#include <pxr/imaging/hd/mesh.h>

namespace hdMoonray {

/// A representation of a subdivision surface or poly-mesh object.

class Mesh final : public pxr::HdMesh, public Geometry
{
public:
    explicit Mesh(pxr::SdfPath const& id INSTANCERID(const pxr::SdfPath&));
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
    void _InitRepr(pxr::TfToken const &reprToken, pxr::HdDirtyBits *dirtyBits) override;

    // Expand dirty bits to what Sync() actually needs
    pxr::HdDirtyBits _PropagateDirtyBits(pxr::HdDirtyBits bits) const override;
    bool isSubdiv(RenderDelegate& renderDelegate);
    void updateCryptomatte(RenderDelegate& renderDelegate);


private:
    // cached values that are combined to produce some settings
    enum SubdivType {NONE = 0, CATCLARK, BILINEAR};
    bool mFlip = false; // leftHanded orientation
    bool mSubd = true; // RdlMeshGeometry default is_subd value
    // This class does not support copying.
    Mesh(const Mesh&)             = delete;
    Mesh &operator =(const Mesh&) = delete;
};

}
