// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <tbb/tbb_machine.h> // fix for icc-19/tbb bug
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/rprim.h>

namespace scene_rdl2 { namespace rdl2 {
class SceneObject;
class Material;
class Displacement;
class VolumeShader;
struct LayerAssignment;
}}

namespace hdMoonray {

class RenderDelegate;

/// Material for an rprim

class Material: public pxr::HdMaterial
{
public:
    explicit Material(pxr::SdfPath const& id): pxr::HdMaterial(id) {}

    /// Dirty bits to pass to first call to Sync()
    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    /// Update the data identified by dirtyBits. Must not query other data.
    void Sync(pxr::HdSceneDelegate* sceneDelegate,
              pxr::HdRenderParam*   renderParam,
              pxr::HdDirtyBits*     dirtyBits) override;

#if PXR_VERSION < 2011
    /// removed in 0.20.11
    void Reload() override {}
#endif

    // True if usdview has enabled scene material
    bool isEnabled() const;

    // Returns the "surface" terminal. Returns default if blank, error material if not found or other error
    scene_rdl2::rdl2::Material* getMaterial(RenderDelegate&, pxr::HdSceneDelegate*, const pxr::HdRprim*);
    // Returns the "displacement" terminal
    scene_rdl2::rdl2::Displacement* getDisplacement(RenderDelegate&, pxr::HdSceneDelegate*, const pxr::HdRprim*);
    // Returns the "volume" terminal
    scene_rdl2::rdl2::VolumeShader* getVolumeShader(RenderDelegate&, pxr::HdSceneDelegate*, const pxr::HdRprim*);
    // Get all of them into a LayerAssignment
    static void get(scene_rdl2::rdl2::LayerAssignment&, const pxr::SdfPath&, RenderDelegate&, pxr::HdSceneDelegate*,
                    const pxr::HdRprim*, bool volume = false);

private:
    scene_rdl2::rdl2::SceneObject* updateTerminal(pxr::TfToken terminal, RenderDelegate&, pxr::HdSceneDelegate*, const pxr::HdRprim*);
    pxr::VtValue mResource;
    bool mMaterialDirty;
    bool mDisplacementDirty;
    bool mVolumeShaderDirty;
    scene_rdl2::rdl2::Material* mMaterial = nullptr;
    scene_rdl2::rdl2::Displacement* mDisplacement = nullptr;
    scene_rdl2::rdl2::VolumeShader* mVolumeShader = nullptr;
    const pxr::HdRprim* mGeom = nullptr;
    std::mutex mCreateMutex;
};

}

void
dumpMaterialNetworkMap(const pxr::HdMaterialNetworkMap&);

scene_rdl2::rdl2::SceneObject*
getCoordSysBinding(
    hdMoonray::RenderDelegate&,
    pxr::HdSceneDelegate*,
    const pxr::HdMaterialNode& , pxr::TfToken ,
    const pxr::HdRprim*
);
scene_rdl2::rdl2::SceneObject*
makeMoonrayShader(
    hdMoonray::RenderDelegate& ,
    pxr::HdSceneDelegate*,
    const pxr::HdMaterialNode&,
    const std::string& nodeName,
    const pxr::HdRprim*
);
