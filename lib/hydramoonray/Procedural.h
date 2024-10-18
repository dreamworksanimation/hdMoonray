// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "GeometryMixin.h"

// Handles any Moonray geometry shader : the RDL class name should be specified by
// procedural:class. Arbitrary RDL2 attribute "xyz" can be set by USD attribute 
// procedural:xyz.
//
// Requires an adapter to work with usdImaging : the adapter should override
// get(TfToken("parts")) to return a perPartMaterials ( == vector<pair<SdfPath,SdfPath>>)
// where each entry is the id of a part and the id of a material.

namespace hdMoonray {

class Procedural final : public pxr::HdRprim, public GeometryMixin
{
public:
    explicit Procedural(pxr::SdfPath const& id);
    
    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    void Sync(pxr::HdSceneDelegate* sceneDelegate,
              pxr::HdRenderParam*   renderParam,
              pxr::HdDirtyBits*     dirtyBits,
              const pxr::TfToken&   reprToken) override;

    void Finalize(pxr::HdRenderParam* renderParam) override;

    const pxr::TfTokenVector& GetBuiltinPrimvarNames() const override;

protected:

    // Hydra overrides
    void _InitRepr(pxr::TfToken const &reprToken, pxr::HdDirtyBits *dirtyBits) override;
    pxr::HdDirtyBits _PropagateDirtyBits(pxr::HdDirtyBits bits) const override { return bits; }

    // GeometryMixin overrides
    void syncAttributes(pxr::HdSceneDelegate* sceneDelegate, RenderDelegate& renderDelegate,
                        pxr::HdDirtyBits* dirtyBits, const pxr::TfToken& reprToken) override;
    bool supportsUserData() override;

private:
    Procedural(const Procedural&)             = delete;
    Procedural &operator =(const Procedural&) = delete;

};

}
