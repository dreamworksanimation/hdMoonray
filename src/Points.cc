// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Points.h"
#include "RenderDelegate.h"
#include "HdmLog.h"

#include <pxr/base/gf/vec2f.h>

#include <scene_rdl2/scene/rdl2/Geometry.h>

#include <iostream>

namespace hdMoonray {

using scene_rdl2::logging::Logger;

Points::Points(pxr::SdfPath const& id INSTANCERID(const pxr::SdfPath& iid)):
    pxr::HdPoints(id INSTANCERID(iid)), Geometry(this) {}

const std::string&
Points::className(pxr::HdSceneDelegate* sceneDelegate) const {
    static const std::string r("RdlPointGeometry");
    return r;
}

void
Points::Finalize(pxr::HdRenderParam* renderParam)
{
    // std::cout << GetId() << " finalize\n";
    finalize(RenderDelegate::get(renderParam));
}

pxr::HdDirtyBits
Points::GetInitialDirtyBitsMask() const
{
    int mask = pxr::HdChangeTracker::Clean
        | pxr::HdChangeTracker::DirtyPoints
        | pxr::HdChangeTracker::DirtyPrimID
        | pxr::HdChangeTracker::DirtyPrimvar
        | pxr::HdChangeTracker::DirtyMaterialId
        | pxr::HdChangeTracker::DirtyTransform
        | pxr::HdChangeTracker::DirtyVisibility
        | pxr::HdChangeTracker::DirtyWidths
        | pxr::HdChangeTracker::DirtyDoubleSided
        | pxr::HdChangeTracker::DirtyInstancer
        | pxr::HdChangeTracker::DirtyInstanceIndex
        | pxr::HdChangeTracker::DirtyCategories
        ;
    return (pxr::HdDirtyBits)mask;
}

void
Points::Sync(pxr::HdSceneDelegate *sceneDelegate,
                  pxr::HdRenderParam   *renderParam,
                  pxr::HdDirtyBits     *dirtyBits,
                  pxr::TfToken const   &reprToken)
{
    const pxr::SdfPath& id = GetId();
    hdmLogSyncStart("Points", id, dirtyBits);

    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));
    renderDelegate.setStartTime();

    if (pxr::HdChangeTracker::IsVisibilityDirty(*dirtyBits, id))
        _UpdateVisibility(sceneDelegate, dirtyBits);
    Geometry::DirtyPrimvars dirtyPrimvars(getDirtyPrimvars(sceneDelegate, renderDelegate, dirtyBits));

    if (not syncCreateGeometry(sceneDelegate, renderDelegate, dirtyBits, reprToken))
        return;

    {   UpdateGuard guard(renderDelegate, geometry());
        setCommonAttributes(sceneDelegate, renderDelegate, dirtyBits, dirtyPrimvars);

        // HdPoints has no support for GeomSubsets, so we ignore them
        // and don't generate any parts

        for (auto& i : dirtyPrimvars) {
            pxr::TfToken name = i.first;
            pxr::VtValue& value = i.second.value;

            if (!updateCommonPrimvars(sceneDelegate, id, name, value, geometry())) {
                if (name ==  pxr::HdTokens->widths) {
                    const pxr::VtFloatArray& v = value.Get<pxr::VtFloatArray>();
                    scene_rdl2::rdl2::FloatVector w;
                    w.reserve(v.size());
                    for (float width : v) w.emplace_back(width/2);
                    geometry()->set("radius_list", w);
                    value = pxr::VtValue(); // remove it from UserData primvars
                }
            }
        }
    }

    updatePrimvars(dirtyPrimvars, renderDelegate);

#if PXR_VERSION >= 2102
    _UpdateInstancer(sceneDelegate, dirtyBits);
#endif
#if PXR_VERSION < 2105
    if (*dirtyBits & pxr::HdChangeTracker::DirtyMaterialId) {
        _SetMaterialId(sceneDelegate->GetRenderIndex().GetChangeTracker(), sceneDelegate->GetMaterialId(id));
    }
#endif
    assign(sceneDelegate, renderDelegate, dirtyBits);

    *dirtyBits &= ~pxr::HdChangeTracker::AllSceneDirtyBits;
    hdmLogSyncEnd(id);
}

}
