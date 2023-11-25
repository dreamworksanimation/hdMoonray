// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/imaging/hd/sprim.h>
#include <pxr/imaging/hd/material.h>
#include <scene_rdl2/scene/rdl2/LightFilter.h>

#include <scene_rdl2/scene/rdl2/Types.h>

namespace scene_rdl2 {namespace rdl2 {
class LightFilter;
} }

namespace hdMoonray {

class RenderDelegate;

class LightFilter : public pxr::HdSprim
{
public:
    LightFilter(const pxr::TfToken& type, const pxr::SdfPath& id):
      pxr::HdSprim(id), mType(type) {}

    /// Dirty bits to pass to first call to Sync()
    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    /// Update the data identified by dirtyBits. Must not query other data.
    void Sync(pxr::HdSceneDelegate* sceneDelegate,
              pxr::HdRenderParam*   renderParam,
              pxr::HdDirtyBits*     dirtyBits) override;

    const pxr::TfToken& type() const { return mType; }

    static scene_rdl2::rdl2::LightFilter* getFilter(pxr::HdSceneDelegate*,
                                               RenderDelegate&,
                                               const pxr::SdfPath&);

    void Finalize(pxr::HdRenderParam *renderParam) override;

private:
    std::string rdlClassName(const pxr::SdfPath& id,
                             pxr::HdSceneDelegate *sceneDelegate);
    void syncParams(const pxr::SdfPath& id,
                    pxr::HdSceneDelegate *sceneDelegate,
                    RenderDelegate& renderDelegate);
    void syncTexture(const std::string& attrName,
                    const pxr::SdfPath& id,
                    pxr::HdSceneDelegate *sceneDelegate,
                    RenderDelegate& renderDelegate);
    scene_rdl2::rdl2::LightFilter* getOrCreateFilter(pxr::HdSceneDelegate *sceneDelegate,
                                                RenderDelegate& renderDelegate,
                                               const pxr::SdfPath& filterId);

    pxr::TfToken mType;
    scene_rdl2::rdl2::LightFilter* mLightFilter = nullptr;
    std::mutex mCreateMutex;

    // the name of a category holding all geometry filtered by this light
    pxr::TfToken mLightFilterCategory;

};

}

