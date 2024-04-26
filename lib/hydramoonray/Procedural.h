// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Geometry.h"

namespace hdMoonray {

class Procedural final : public pxr::HdRprim, public Geometry
{
public:
    explicit Procedural(pxr::SdfPath const& id INSTANCERID(const pxr::SdfPath&));
    const std::string& className(pxr::HdSceneDelegate* sceneDelegate) const override;

    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    void Sync(pxr::HdSceneDelegate* sceneDelegate,
              pxr::HdRenderParam*   renderParam,
              pxr::HdDirtyBits*     dirtyBits,
              const pxr::TfToken&   reprToken) override;

    void Finalize(pxr::HdRenderParam* renderParam) override;

    const pxr::TfTokenVector& GetBuiltinPrimvarNames() const override;

protected:
    // Initialize one of the reprs, called before Sync()
    void _InitRepr(pxr::TfToken const &reprToken, pxr::HdDirtyBits *dirtyBits) override;
    void setPruneFlag(RenderDelegate& renderDelegate,
                      pxr::HdSceneDelegate* sceneDelegate);

    // Expand dirty bits to what Sync() actually needs
    pxr::HdDirtyBits _PropagateDirtyBits(pxr::HdDirtyBits bits) const override { return bits; }

private:
    std::string mClassName;
    Procedural(const Procedural&)             = delete;
    Procedural &operator =(const Procedural&) = delete;
    bool hasPrimitiveAttributes = false;
    unsigned mPruned = 0; // display option for pruning procedural geometries.

};

}
