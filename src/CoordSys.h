// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <tbb/tbb_machine.h> // fix for icc-19/tbb bug
#include <pxr/imaging/hd/coordSys.h>

namespace scene_rdl2 {namespace rdl2 { class SceneObject; } }

namespace hdMoonray {

class RenderDelegate;

/// Coordinate system attached to an rprim. In Moonray the coordinate system is a shader node
/// so currently the if several objects with different Coordinate systems use the same shader,
/// it will use one of those coordinate systems for all of them
class CoordSys: public pxr::HdCoordSys
{
public:
    explicit CoordSys(pxr::SdfPath const& id);

    /// Dirty bits to pass to first call to Sync()
    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override { return DirtyBits::AllDirty; }

    /// Update the data identified by dirtyBits. Must not query other data.
    void Sync(pxr::HdSceneDelegate* sceneDelegate,
              pxr::HdRenderParam*   renderParam,
              pxr::HdDirtyBits*     dirtyBits) override;

    /// Guess at the binding based on geometry and coordSys name, to work around Hydra failings
    static scene_rdl2::rdl2::SceneObject* getBinding(
        pxr::HdSceneDelegate*, RenderDelegate&,
        const pxr::SdfPath& geomPath, pxr::TfToken name);
};

}

