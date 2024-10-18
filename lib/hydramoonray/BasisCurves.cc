// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

// BasisCurves are implemented using RdlCurveGeometry
// The following features are supported:
//
// - linear, bezier and bspline bases (NOT Catmull Rom)
// - nonperiodic only, periodic curves are NOT supported
// - non-indexed verts only, indexed verts are NOT supported
//
// Tessellation rate and roundness are set based on the refineLevel
// display style ("complexity" in usd_view). As with other RDL geometry
// attributes, these can be overridden by a primvar.

#include "BasisCurves.h"
#include "RenderDelegate.h"
#include "HdmLog.h"
#include <pxr/base/gf/vec2f.h>
#include <scene_rdl2/scene/rdl2/Geometry.h>
#include <iostream>

using namespace pxr;

namespace {

// round curves are more expensive than rayFacing, so are not generated
// at this refine level and below
constexpr int REFINE_LEVEL_FLAT_CURVES = 2;
// tessellation rate used at refine level 0
constexpr int TESS_RATE_LOW = 1;
// tessellation rate used at refine level greater than 0
constexpr int TESS_RATE_NORMAL = 4;

// constants for RDL attrs and enums used herein
const std::string rdlClassCurves("RdlCurveGeometry");
const std::string rdlAttrCurveType("curve_type");
constexpr int rdlCurveType_linear = 0;
constexpr int rdlCurveType_bezier = 1;
constexpr int rdlCurveType_bspline = 2;
const std::string rdlAttrCurvesSubtype("curves_subtype");
constexpr int rdlCurvesSubtype_rayFacing = 0;
constexpr int rdlCurvesSubtype_round = 1;
constexpr int rdlCurvesSubtype_normalOriented = 2;
const std::string rdlAttrCurveVertexCounts("curves_vertex_count");
const std::string rdlAttrTessellationRate("tessellation_rate");
const std::string rdlAttrReverseNormals("reverse_normals");
const std::string rdlAttrUvList("uv_list");
const std::string rdlAttrRadiusList("radius_list");

}

namespace hdMoonray {

using scene_rdl2::logging::Logger;
using scene_rdl2::rdl2::IntVector;
using scene_rdl2::rdl2::FloatVector;
using scene_rdl2::rdl2::Vec2f;
using scene_rdl2::rdl2::Vec2fVector;

BasisCurves::BasisCurves(SdfPath const& id) :
    HdBasisCurves(id), 
    GeometryMixin(this) 
{}

void
BasisCurves::Finalize(HdRenderParam* renderParam)
{
    // causes geometry to be hidden
    resetGeometryObject(RenderDelegate::get(renderParam));
}

HdDirtyBits
BasisCurves::GetInitialDirtyBitsMask() const
{
    int mask = HdChangeTracker::Clean
        | HdChangeTracker::DirtyPrimID
        | HdChangeTracker::DirtyDisplayStyle
        | HdChangeTracker::DirtyPrimvar
        | HdChangeTracker::DirtyMaterialId
        | HdChangeTracker::DirtyTopology
        | HdChangeTracker::DirtyTransform
        | HdChangeTracker::DirtyVisibility
        | HdChangeTracker::DirtyDoubleSided
        | HdChangeTracker::DirtySubdivTags
        | HdChangeTracker::DirtyWidths
        | HdChangeTracker::DirtyInstancer
        | HdChangeTracker::DirtyInstanceIndex
        | HdChangeTracker::DirtyCategories;
    return (HdDirtyBits)mask;
}

void
BasisCurves::syncTopology(const HdBasisCurvesTopology& topology)
{
    VtIntArray curveVertexCounts = topology.GetCurveVertexCounts();
    const int* p = reinterpret_cast<const int*>(&curveVertexCounts[0]);
    geometry()->set(rdlAttrCurveVertexCounts, IntVector(p, p + curveVertexCounts.size()));

    if (topology.HasIndices()) {
        Logger::error(GetId(), ": curve indices are not supported");
    }

    // note Hydra has separate type (linear, cubic) and basis (bezier, bSpline, catmullRom)
    // where basis is ignored for linear type. RDL has a single type enum (linear, bezier, bspline)
    const TfToken curveType(topology.GetCurveType());
    const TfToken curveBasis(topology.GetCurveBasis());
    int rdlCurveType(rdlCurveType_bspline);
    if (curveType == HdTokens->linear)  rdlCurveType = rdlCurveType_linear;
    else if (curveBasis == HdTokens->bezier) rdlCurveType = rdlCurveType_bezier;
    else if (curveBasis == HdTokens->bSpline) rdlCurveType = rdlCurveType_bspline;
    else Logger::error(GetId(), ": unsupported curve basis '", curveBasis, "'");
    geometry()->set(rdlAttrCurveType, rdlCurveType);

    if (topology.GetCurveWrap() != HdTokens->nonperiodic) {
        Logger::error(GetId(), ": unsupported curve wrap '", topology.GetCurveWrap(), "'");
    }

    geometry()->set(rdlAttrReverseNormals, bool(isMirror()));
}

void
BasisCurves::syncDisplayStyle(const HdDisplayStyle& style)
{
    // refineLevel is set by the "complexity" menu item in usdview
    // and is an integer 0 to 8. At low refineLevel, we simplify the geometry
    // to improve render performance. These defaults can be overridden by primvars
    // moonray:curves_subtype and moonray::tessellation_rate
    static TfToken curvesSubtypeToken("moonray:curves_subtype");
    static TfToken tessellationRateToken("moonray:tessellation_rate");
    if (not isPrimvarUsed(curvesSubtypeToken)) {
        bool round = style.refineLevel > REFINE_LEVEL_FLAT_CURVES;
        geometry()->set(rdlAttrCurvesSubtype, round ? rdlCurvesSubtype_round : 
                                                      rdlCurvesSubtype_rayFacing);
    }
    if (not isPrimvarUsed(tessellationRateToken)) {
        int tessellationRate = (style.refineLevel < 1) ? TESS_RATE_LOW : TESS_RATE_NORMAL;
        geometry()->set(rdlAttrTessellationRate, tessellationRate);
    }
}

void 
BasisCurves::syncAttributes(HdSceneDelegate* sceneDelegate, 
                            RenderDelegate& renderDelegate,
                            HdDirtyBits* dirtyBits,
                            const TfToken& reprToken) 
{    
    // Overridden from GeometryMixin to sync additional data in the
    // BasicCurves schema to RDL attributes. Will be called from syncAll()
    if (HdChangeTracker::IsTopologyDirty(*dirtyBits, GetId())) {
        syncTopology(GetBasisCurvesTopology(sceneDelegate));
    }    
        
    if (HdChangeTracker::IsDisplayStyleDirty(*dirtyBits, GetId()) ||
        HdChangeTracker::IsTopologyDirty(*dirtyBits, GetId())) {
        syncDisplayStyle(GetDisplayStyle(sceneDelegate));
    }

    // base class handles transform and double-sided
    GeometryMixin::syncAttributes(sceneDelegate, renderDelegate, dirtyBits, reprToken);
}

void
BasisCurves::primvarChanged(HdSceneDelegate *sceneDelegate, 
                            RenderDelegate& renderDelegate,
                            const TfToken& name,  
                            const VtValue& value,
                            const HdInterpolation& interp, 
                            const TfToken& role)
{
    // Overridden from GeometryMixin to sync primvars that drive RDL object
    // attributes. Will be called from syncAll().
    // RdlCurveGeometry supports st/uv and widths via
    // object attributes

    static const TfToken stToken("st");
    static const TfToken uvToken("uv");

    if (name == stToken || name == uvToken) {
        if (value.IsEmpty()) {
            geometry()->resetToDefault(rdlAttrUvList);
        } else if (value.IsHolding<VtVec2fArray>()) {
            const VtVec2fArray& v = value.UncheckedGet<VtVec2fArray>();
            const Vec2f* p = reinterpret_cast<const Vec2f*>(&v[0]);
            geometry()->set(rdlAttrUvList, Vec2fVector(p, p + v.size()));
        }
    } else if (name ==  HdTokens->widths) {
        if (value.IsEmpty()) {
            geometry()->resetToDefault(rdlAttrRadiusList);
        } else if (value.IsHolding<VtFloatArray>()) {
            const VtFloatArray& v = value.UncheckedGet<VtFloatArray>();
            FloatVector w{v.begin(), v.end()};
            // width is diameter : convert to radius
            for (float& r : w) r /= 2;
            geometry()->set(rdlAttrRadiusList, std::move(w));
        }
    } else {
        // allow GeometryMixin to handle all other cases, including
        // primvar overrides and UserData
        GeometryMixin::primvarChanged(sceneDelegate, renderDelegate,
                                      name, value, interp, role);
    }
}

void
BasisCurves::Sync(HdSceneDelegate *sceneDelegate,
                  HdRenderParam   *renderParam,
                  HdDirtyBits     *dirtyBits,
                  TfToken const   &reprToken)
{
    hdmLogSyncStart("BasisCurves", GetId(), dirtyBits);   
    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));
    
    _UpdateVisibility(sceneDelegate, dirtyBits);
    _UpdateInstancer(sceneDelegate, dirtyBits);
    
    syncAll(rdlClassCurves, sceneDelegate, renderDelegate, dirtyBits, reprToken);
    
    hdmLogSyncEnd(GetId());
}

}
