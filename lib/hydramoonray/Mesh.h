// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "GeometryMixin.h"
#include <pxr/imaging/hd/mesh.h>

namespace hdMoonray {

/// A representation of a subdivision surface or poly-mesh object.

class Mesh final : public pxr::HdMesh, public GeometryMixin
{
public:
    explicit Mesh(pxr::SdfPath const& id);

    /// Dirty bits to pass to first call to Sync()
    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    /// Update the data identified by dirtyBits. Must not query other data.
    void Sync(pxr::HdSceneDelegate* sceneDelegate,
              pxr::HdRenderParam*   renderParam,
              pxr::HdDirtyBits*     dirtyBits,
              const pxr::TfToken&   reprToken) override;

    void Finalize(pxr::HdRenderParam* renderParam) override;

    // used to get the geometry associated with a MeshLight
    // not threadsafe, use with caution
    scene_rdl2::rdl2::Geometry* geometryForMeshLight(const RenderDelegate& renderDelegate);

protected:
    // Hydra overrides
    void _InitRepr(pxr::TfToken const &reprToken, pxr::HdDirtyBits *dirtyBits) override;
    pxr::HdDirtyBits _PropagateDirtyBits(pxr::HdDirtyBits bits) const override;

    // GeometryMixin overrides
    void syncAttributes(pxr::HdSceneDelegate* sceneDelegate, RenderDelegate& renderDelegate,
                        pxr::HdDirtyBits* dirtyBits, const pxr::TfToken& reprToken) override;
    void primvarChanged(pxr::HdSceneDelegate *sceneDelegate, RenderDelegate& renderDelegate,
                        const pxr::TfToken& name,  const pxr::VtValue& value,
                        const pxr::HdInterpolation& interp, const pxr::TfToken& role) override;
private:
    Mesh(const Mesh&)             = delete;
    Mesh &operator =(const Mesh&) = delete;

    void syncTopology(const pxr::HdMeshTopology& topology);
    void syncSubdivScheme(const pxr::HdMeshTopology& topology,
                        pxr::HdSceneDelegate *sceneDelegate,
                        const RenderDelegate& renderDelegate,
                        const pxr::TfToken& reprToken);
    void syncSubdivTags(const pxr::PxOsdSubdivTags& tags);
    void syncCryptomatteUserData(RenderDelegate& renderDelegate);
};

}
