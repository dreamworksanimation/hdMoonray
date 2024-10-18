// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

// Points are implemented using RdlPointGeometry
// Very similar to (but simpler than) BasisCurves

#include "Points.h"
#include "RenderDelegate.h"
#include "HdmLog.h"
#include <pxr/base/gf/vec2f.h>
#include <scene_rdl2/scene/rdl2/Geometry.h>
#include <iostream>

using namespace pxr;

namespace {

// constants for RDL attrs and enums used herein
const std::string rdlClassPoints("RdlPointGeometry");
const std::string rdlAttrRadiusList("radius_list");

}

namespace hdMoonray {

using scene_rdl2::logging::Logger;
using scene_rdl2::rdl2::FloatVector;

Points::Points(SdfPath const& id):
    HdPoints(id),
    GeometryMixin(this) 
{}

void
Points::Finalize(HdRenderParam* renderParam)
{
    // causes geometry to be hidden
    resetGeometryObject(RenderDelegate::get(renderParam));
}

HdDirtyBits
Points::GetInitialDirtyBitsMask() const
{
    int mask = HdChangeTracker::Clean
        | HdChangeTracker::DirtyPoints
        | HdChangeTracker::DirtyPrimID
        | HdChangeTracker::DirtyPrimvar
        | HdChangeTracker::DirtyMaterialId
        | HdChangeTracker::DirtyTransform
        | HdChangeTracker::DirtyVisibility
        | HdChangeTracker::DirtyWidths
        | HdChangeTracker::DirtyDoubleSided
        | HdChangeTracker::DirtyInstancer
        | HdChangeTracker::DirtyInstanceIndex
        | HdChangeTracker::DirtyCategories;
    return (HdDirtyBits)mask;
}

void
Points::primvarChanged(HdSceneDelegate *sceneDelegate, RenderDelegate& renderDelegate,
                       const TfToken& name,  const VtValue& value,
                       const HdInterpolation& interp, const TfToken& role)
{
    // Handles primvars that drive RDL attributes, rather than creating UserData
    // RdlPointGeometry directly supports primvar "widths" via
    // object attribute "radius_list"
    if (name ==  HdTokens->widths) {
        if (value.IsEmpty()) {
            geometry()->resetToDefault(rdlAttrRadiusList);
        } else if (value.IsHolding<VtFloatArray>()) {
            const VtFloatArray& v = value.UncheckedGet<VtFloatArray>();
                FloatVector w(v.begin(), v.end());
                // width is diameter : convert to radius
                for (float& r : w) r /= 2;
                geometry()->set(rdlAttrRadiusList, std::move(w));
        }
    } else {
        // allow GeometryMixin to handle all other cases
        GeometryMixin::primvarChanged(sceneDelegate, renderDelegate,
                                      name, value, interp, role);
    }
}

void
Points::Sync(HdSceneDelegate *sceneDelegate,
             HdRenderParam   *renderParam,
             HdDirtyBits     *dirtyBits,
             TfToken const   &reprToken)
{
    hdmLogSyncStart("Points", GetId(), dirtyBits);   
    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));

    _UpdateVisibility(sceneDelegate, dirtyBits);
    _UpdateInstancer(sceneDelegate, dirtyBits);
    
    syncAll(rdlClassPoints, sceneDelegate, renderDelegate, dirtyBits, reprToken);
    
    hdmLogSyncEnd(GetId());
}

}
