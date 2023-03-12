// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "BasisCurves.h"
#include "RenderDelegate.h"

#include <pxr/base/gf/vec2f.h>

#include <scene_rdl2/scene/rdl2/Geometry.h>

#include <iostream>

namespace {
// RdlCurve enums
constexpr int CURVE_TYPE_LINEAR = 0;
constexpr int CURVE_TYPE_BEZIER = 1;
constexpr int CURVE_TYPE_BSPLINE = 2;

const pxr::TfToken curve_subtypeToken("moonray:curve_subtype");
const pxr::TfToken roundToken("round");
const pxr::TfToken tessellation_rateToken("moonray:tessellation_rate");
}

namespace hdMoonray {

using scene_rdl2::logging::Logger;

BasisCurves::BasisCurves(pxr::SdfPath const& id INSTANCERID(pxr::SdfPath const& iid)):
    pxr::HdBasisCurves(id INSTANCERID(iid)), Geometry(this) {}

const std::string&
BasisCurves::className(pxr::HdSceneDelegate* sceneDelegate) const {
    static const std::string r("RdlCurveGeometry");
    return r;
}

void
BasisCurves::Finalize(pxr::HdRenderParam* renderParam)
{
    // std::cout << GetId() << " finalize\n";
    finalize(RenderDelegate::get(renderParam));
}

pxr::HdDirtyBits
BasisCurves::GetInitialDirtyBitsMask() const
{
    int mask = pxr::HdChangeTracker::Clean
        | pxr::HdChangeTracker::DirtyPrimID
        //| pxr::HdChangeTracker::DirtyExtent
        | pxr::HdChangeTracker::DirtyDisplayStyle
        //| pxr::HdChangeTracker::DirtyPoints
        | pxr::HdChangeTracker::DirtyPrimvar
        | pxr::HdChangeTracker::DirtyMaterialId
        | pxr::HdChangeTracker::DirtyTopology
        | pxr::HdChangeTracker::DirtyTransform
        | pxr::HdChangeTracker::DirtyVisibility
        //| pxr::HdChangeTracker::DirtyNormals
        | pxr::HdChangeTracker::DirtyDoubleSided
        //| pxr::HdChangeTracker::DirtyCullStyle
        | pxr::HdChangeTracker::DirtySubdivTags
        | pxr::HdChangeTracker::DirtyWidths
        | pxr::HdChangeTracker::DirtyInstancer
        | pxr::HdChangeTracker::DirtyInstanceIndex
        | pxr::HdChangeTracker::DirtyCategories
        //| pxr::HdChangeTracker::DirtyRenderTag
        //| pxr::HdChangeTracker::DirtyComputationPrimvarDesc
        //| pxr::HdChangeTracker::DirtyCategories
        //| pxr::HdChangeTracker::DirtyVolumeField
        ;
    return (pxr::HdDirtyBits)mask;
}

void
BasisCurves::Sync(pxr::HdSceneDelegate *sceneDelegate,
                  pxr::HdRenderParam   *renderParam,
                  pxr::HdDirtyBits     *dirtyBits,
                  pxr::TfToken const   &reprToken)
{
    const pxr::SdfPath& id = GetId();
    // std::cout << id << " Sync dirtyBits=" << std::hex << *dirtyBits << std::endl;
    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));
    renderDelegate.setStartTime();

    if (pxr::HdChangeTracker::IsVisibilityDirty(*dirtyBits, id))
        _UpdateVisibility(sceneDelegate, dirtyBits);
    Geometry::DirtyPrimvars dirtyPrimvars(getDirtyPrimvars(sceneDelegate, renderDelegate, dirtyBits));

    if (not syncCreateGeometry(sceneDelegate, renderDelegate, dirtyBits, reprToken))
        return;

    {   UpdateGuard guard(renderDelegate, geometry());
        setCommonAttributes(sceneDelegate, renderDelegate, dirtyBits, dirtyPrimvars);

        // Topology //////////////////////////////////////////////////////////////

        pxr::VtIntArray curveVertexCounts;
        if (pxr::HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) {
            pxr::HdBasisCurvesTopology topology(GetBasisCurvesTopology(sceneDelegate));

            curveVertexCounts = topology.GetCurveVertexCounts();
            const int* p = reinterpret_cast<const int*>(&curveVertexCounts[0]);
            geometry()->set("curves_vertex_count", scene_rdl2::rdl2::IntVector(p, p + curveVertexCounts.size()));

            if (topology.HasIndices()) {
                // RdlCurveGeometry doesn't directly support indexed verts
                // we could support indices by expanding out vertex list,
                // but leave unsupported for now
                Logger::error(id, ": curve indices are not supported");
            }

            // HdMeshTopology has no support for GeomSubsets, so we ignore them
            // and don't generate any parts

            if (topology.GetCurveType() == pxr::HdTokens->linear) {
                geometry()->set("curve_type", CURVE_TYPE_LINEAR);
            } else if (topology.GetCurveBasis() == pxr::HdTokens->bezier) {
                geometry()->set("curve_type", CURVE_TYPE_BEZIER);
            } else {
                if (topology.GetCurveBasis() != pxr::HdTokens->bSpline)
                    Logger::error(id, ": unsupported curve basis '", topology.GetCurveBasis(), "'");
                // the missing one is CatmullRom, use bspline as it uses the same number of vertices
                geometry()->set("curve_type", CURVE_TYPE_BSPLINE);
            }

            if (topology.GetCurveWrap() != pxr::HdTokens->nonperiodic) {
                // unsupported for now
                Logger::error(id, ".wrap: ", topology.GetCurveWrap(), " not supported");
            }

            geometry()->set("reverse_normals", bool(isMirror()));
        }

        if (pxr::HdChangeTracker::IsDisplayStyleDirty(*dirtyBits, id) ||
            pxr::HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) {
            // The DisplayStyle refineLevel is set by the "complexity" menu item in usdview
            int refineLevel = GetDisplayStyle(sceneDelegate).refineLevel;
            bool round;
            int tessellation_rate;
            if (refineLevel < 1) {
                round = false;
                tessellation_rate = 1;
            } else {
                pxr::VtValue v = get(curve_subtypeToken, sceneDelegate);
                if (not v.IsEmpty()) {
                    round = v.Get<pxr::TfToken>() == roundToken;
                } else {
                    round = refineLevel > 2;
                }
                v = get(tessellation_rateToken, sceneDelegate);
                if (not v.IsEmpty()) {
                    tessellation_rate = v.Get<int>();
                } else {
                    tessellation_rate = 4;
                }
            }
            geometry()->set("curve_subtype", round ? 1 : 0);
            geometry()->set("tessellation_rate", tessellation_rate);
        }

        // Primvars //////////////////////////////////////////////////////////////

        static const pxr::TfToken stToken("st");
        static const pxr::TfToken uvToken("uv");

        for (auto& i : dirtyPrimvars) {
            pxr::TfToken name = i.first;
            pxr::VtValue& value = i.second.value;

            if (!updateCommonPrimvars(sceneDelegate, id, name, value, geometry())) {
                if (name == stToken || name == uvToken) {
                    const pxr::VtVec2fArray& v = value.Get<pxr::VtVec2fArray>();
                    const scene_rdl2::rdl2::Vec2f* p = reinterpret_cast<const scene_rdl2::rdl2::Vec2f*>(&v[0]);
                    geometry()->set("uv_list", scene_rdl2::rdl2::Vec2fVector(p, p + v.size()));
                    value = pxr::VtValue(); // remove it from UserData primvars

                } else if (name ==  pxr::HdTokens->widths) {
                    const pxr::VtFloatArray& v = value.Get<pxr::VtFloatArray>();
                    scene_rdl2::rdl2::FloatVector w{v.begin(), v.end()};
                    for (float& r : w) r /= 2;
                    geometry()->set("radius_list", std::move(w));
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
}

}
