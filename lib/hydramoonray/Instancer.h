// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <tbb/tbb_machine.h> // fix for icc-19/tbb bug
#include <pxr/imaging/hd/instancer.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/tf/hashmap.h>
#include <pxr/imaging/hd/vtBufferSource.h>

#include <scene_rdl2/scene/rdl2/SceneObject.h>

#include "GeometryMixin.h"
#include "MurmurHash3.h"

#include <mutex>

namespace hdMoonray {

class Instancer: public pxr::HdInstancer
{
public:
    Instancer(pxr::HdSceneDelegate* delegate,
              const pxr::SdfPath& id);
    ~Instancer();

    // Hydra instancers are stored "backwards", the prototype knows what instance it
    // is in. When the prototype generates rdla_scene objects it calls this to
    // generate an instancer scene object. The id and prototypeId are used to look up
    // all the rest of the information. The instancer pointer is set or the object
    // already there is updated. There do not appear to be any other sync calls to
    // the instancer, so it will have to update transforms and primvars for this
    // prototype in this call.
    void makeInstanceGeometry(const pxr::SdfPath& prototypeId,
                              scene_rdl2::rdl2::Geometry* prototype,
                              GeometryMixin* geometry, size_t level,
                              size_t childCount = 1);

#if PXR_VERSION < 2102
    void sync(pxr::HdSceneDelegate *sceneDelegate,
              pxr::HdDirtyBits     *dirtyBits);
#else
    void Sync(pxr::HdSceneDelegate *sceneDelegate,
              pxr::HdRenderParam   *renderParam,
              pxr::HdDirtyBits     *dirtyBits) override;
#endif

private:

    struct PrimvarInfo { pxr::VtValue value; pxr::TfToken role; };
    std::map<pxr::TfToken, PrimvarInfo> mPrimvars;
    pxr::GfMatrix4d mXform;

    // map from prototype object to the instancer created for it
    std::map<scene_rdl2::rdl2::Geometry*, scene_rdl2::rdl2::Geometry*> mInstancers;
    std::mutex mMapMutex;
};

}
