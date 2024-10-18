// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <tbb/tbb_machine.h> // fix for icc-19/tbb bug
#include <pxr/imaging/hd/light.h>

#include <scene_rdl2/scene/rdl2/Types.h>

namespace scene_rdl2 {namespace rdl2 {
class Light;
} }

namespace hdMoonray {

class RenderDelegate;

class Light: public pxr::HdLight
{
public:
    Light(const pxr::TfToken& type, const pxr::SdfPath& id):
        pxr::HdLight(id), mType(type), mRectToSpotlight(false) {}

    /// Dirty bits to pass to first call to Sync()
    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    /// Update the data identified by dirtyBits. Must not query other data.
    void Sync(pxr::HdSceneDelegate* sceneDelegate,
              pxr::HdRenderParam*   renderParam,
              pxr::HdDirtyBits*     dirtyBits) override;

    const pxr::TfToken& type() const { return mType; }

    static bool isSupportedType(const pxr::TfToken& token);

    void Finalize(pxr::HdRenderParam *renderParam) override;

private:
    const std::string& rdlClassName(const pxr::SdfPath& id,
                                    pxr::HdSceneDelegate *sceneDelegate);

    void syncXform(const pxr::SdfPath& id,
                   pxr::HdSceneDelegate *sceneDelegate);
    void syncParams(const pxr::SdfPath& id,
                    pxr::HdSceneDelegate *sceneDelegate,
                    RenderDelegate& renderDelegate);
    void syncFilterList(const pxr::SdfPath& id,
                        pxr::HdSceneDelegate *sceneDelegate,
                        RenderDelegate& renderDelegate);

    void fixCylinderLight(scene_rdl2::rdl2::Mat4d& mat);

    pxr::TfToken mType;

    // Holds whether this light was converted from a rect light to a spot light
    bool mRectToSpotlight;

    scene_rdl2::rdl2::Light* mLight = nullptr;
    bool mOn = false;
    void setOn(bool, RenderDelegate& renderDelegate);

    // the name of a category holding all geometry lit by this light
    pxr::TfToken mLightLinkCategory;
    pxr::TfToken mShadowLinkCategory;
};

}

