// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Mesh.h"
#include "RenderDelegate.h"
#include "Material.h"

#include <pxr/base/gf/vec2f.h>
#include <pxr/imaging/hd/meshUtil.h>
#include <pxr/imaging/pxOsd/tokens.h>

#include <scene_rdl2/scene/rdl2/Geometry.h>

#include <iostream>

namespace {

// RdlMesh enums
constexpr int SUBD_SCHEME_BILINEAR = 0;
constexpr int SUBD_SCHEME_CATCLARK = 1;
constexpr int SUBD_BOUNDARY_NONE = 0;
constexpr int SUBD_BOUNDARY_EDGE_ONLY = 1;
constexpr int SUBD_BOUNDARY_EDGE_AND_CORNER = 2;
constexpr int SUBD_FVAR_LINEAR_NONE = 0;
constexpr int SUBD_FVAR_LINEAR_CORNERS_ONLY = 1;
constexpr int SUBD_FVAR_LINEAR_CORNERS_PLUS1 = 2;
constexpr int SUBD_FVAR_LINEAR_CORNERS_PLUS2 = 3;
constexpr int SUBD_FVAR_LINEAR_BOUNDARIES = 4;
constexpr int SUBD_FVAR_LINEAR_ALL = 5;

const pxr::TfToken mesh_resolutionToken("moonray:mesh_resolution");
const pxr::TfToken adaptive_errorToken("moonray:adaptive_error");
const pxr::TfToken smooth_normalToken("moonray:smooth_normal");
const pxr::TfToken stToken("st");
const pxr::TfToken uvToken("uv");
const pxr::TfToken normalToken("normal");

}

namespace hdMoonray {

using scene_rdl2::logging::Logger;

Mesh::Mesh(pxr::SdfPath const& id INSTANCERID(const pxr::SdfPath& iid)):
    pxr::HdMesh(id INSTANCERID(iid)), Geometry(this) {}

const std::string&
Mesh::className(pxr::HdSceneDelegate* sceneDelegate) const {
    static const std::string r("RdlMeshGeometry");
    return r;
}

void
Mesh::Finalize(pxr::HdRenderParam* renderParam)
{
    // std::cout << GetId() << " finalize\n";
    finalize(RenderDelegate::get(renderParam));
}

pxr::HdDirtyBits
Mesh::GetInitialDirtyBitsMask() const
{
    int mask = pxr::HdChangeTracker::Clean
        | pxr::HdChangeTracker::InitRepr
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
        //| pxr::HdChangeTracker::DirtyWidths
        | pxr::HdChangeTracker::DirtyInstancer
        | pxr::HdChangeTracker::DirtyInstanceIndex
        //| pxr::HdChangeTracker::DirtyMaterialId
        | pxr::HdChangeTracker::DirtyCategories
        | pxr::HdChangeTracker::DirtyRepr
        //| pxr::HdChangeTracker::DirtyRenderTag
        //| pxr::HdChangeTracker::DirtyComputationPrimvarDesc
        //| pxr::HdChangeTracker::DirtyCategories
        //| pxr::HdChangeTracker::DirtyVolumeField
        ;
    return (pxr::HdDirtyBits)mask;
}

pxr::HdDirtyBits
Mesh::_PropagateDirtyBits(pxr::HdDirtyBits bits) const
{
    return bits;
}

// This seems to be a GL thing to set up a shader. Sync is not called.
// This is only needed if Sync() looks at Repr. Mesh does this to get flat vs smooth usdview render setting.
void
Mesh::_InitRepr(pxr::TfToken const &reprToken, pxr::HdDirtyBits *dirtyBits)
{
    // doing this seems to cause Sync() to be called:
    *dirtyBits |= pxr::HdChangeTracker::DirtyRepr;
}

bool
Mesh::isSubdiv(RenderDelegate& renderDelegate)
{
    if (renderDelegate.getForcePolygon()){
        return false;
    } else {
        // avoid the is_subd value previously set by flat shading or forcePolygon
        if (geometry()->get<int>("subd_scheme")){
            return true;
        }
        return geometry()->get<bool>("is_subd");
    }
}

void
Mesh::Sync(pxr::HdSceneDelegate *sceneDelegate,
           pxr::HdRenderParam   *renderParam,
           pxr::HdDirtyBits     *dirtyBits,
           pxr::TfToken const   &reprToken)
{
    const pxr::SdfPath& id = GetId();
    // std::cout << id << " Sync dirty=" << std::hex << *dirtyBits << std::endl;
    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));
    renderDelegate.setStartTime();

    if (pxr::HdChangeTracker::IsVisibilityDirty(*dirtyBits, id)) {
        _UpdateVisibility(sceneDelegate, dirtyBits);
    }

    Geometry::DirtyPrimvars dirtyPrimvars(getDirtyPrimvars(sceneDelegate,
                                                           renderDelegate,
                                                           dirtyBits));

    if (not syncCreateGeometry(sceneDelegate, renderDelegate, dirtyBits, reprToken)) {
        return; // ignore errors and invisible objects
    }

    {   UpdateGuard guard(renderDelegate, geometry());
        setCommonAttributes(sceneDelegate, renderDelegate, dirtyBits, dirtyPrimvars);

        // Topology //////////////////////////////////////////////////////////////

        // Currently if moonray: attributes are changed the prim is destroyed and recreated,
        // so this code will be called on such changes.
        if (pxr::HdChangeTracker::IsDisplayStyleDirty(*dirtyBits, id) ||
            pxr::HdChangeTracker::IsReprDirty(*dirtyBits, id) ||
            pxr::HdChangeTracker::IsTopologyDirty(*dirtyBits, id) ||
            pxr::HdChangeTracker::IsSubdivTagsDirty(*dirtyBits, id)) {
            pxr::HdMeshTopology topology(GetMeshTopology(sceneDelegate));

            if (pxr::HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) { //
                const pxr::VtIntArray& faceVertexCounts = topology.GetFaceVertexCounts();
                geometry()->set("face_vertex_count",
                                scene_rdl2::rdl2::IntVector(faceVertexCounts.begin(), faceVertexCounts.end()));

                const pxr::VtIntArray& faceVertexIndices = topology.GetFaceVertexIndices();
                geometry()->set("vertices_by_index",
                                scene_rdl2::rdl2::IntVector(faceVertexIndices.begin(), faceVertexIndices.end()));

                mFlip = topology.GetOrientation() != pxr::PxOsdOpenSubdivTokens->rightHanded;
                if (topology.GetOrientation() != pxr::PxOsdOpenSubdivTokens->rightHanded)
                    geometry()->set("orientation", 1);
                // geometry()->set("reverse_normals", flip); // workaround for MOONRAY-3512
                // there is also GetCullStyle() and desc.cullStyle but these are always 0, and it may be correct
                // to ignore them as they are used for multiple-pass rendering.

                const pxr::HdGeomSubsets& geomSubsets(topology.GetGeomSubsets());
                std::vector<int> partFaceCountList;
                std::vector<int> partFaceIndices;
                partFaceCountList.reserve(geomSubsets.size());
                partFaceIndices.reserve(geomSubsets.size()); // actually larger
                partList.clear();
                partList.reserve(geomSubsets.size());
                partMaterials.clear();
                partMaterials.reserve(geomSubsets.size());
                partPaths.clear();
                partPaths.reserve(geomSubsets.size());
                for (const pxr::HdGeomSubset& geomSubset : geomSubsets) {
                    partFaceCountList.push_back(int(geomSubset.indices.size()));
                    for (int i : geomSubset.indices)
                        partFaceIndices.push_back(i);
                    partList.push_back(geomSubset.id.GetName());
                    partMaterials.push_back(geomSubset.materialId);
                    partPaths.push_back(geomSubset.id);
                }
                geometry()->set("part_face_count_list", partFaceCountList);
                geometry()->set("part_face_indices", partFaceIndices);
                geometry()->set("part_list", partList);
            }

            if (_GetReprDesc(reprToken)[0].flatShadingEnabled) {
                // User has chosen "flat shading" from the usdview draw style menu:
                geometry()->set("is_subd", false);
                geometry()->set("smooth_normal", false);

            } else {
                SubdivType subdiv;
                pxr::TfToken subdScheme = topology.GetScheme();
                // Check if "is_subd" has already been set to false by a primvar
                const bool isSubd = isSubdiv(renderDelegate);
                if (!isSubd || subdScheme == pxr::PxOsdOpenSubdivTokens->none)
                    subdiv = NONE;
                else if (subdScheme == pxr::PxOsdOpenSubdivTokens->bilinear)
                    subdiv = BILINEAR;
                else // "catclark" and "loop" ("loop" is not supported)
                    subdiv = CATCLARK;

                // This is the "complexity" menu item in usdview (low=0, medium=1, ...)
                // There is a topology.GetRefineLevel() but it appears to always be a copy of
                // the parameter (which defaults to 0) sent to GetMeshTopology()
                int refineLevel = GetDisplayStyle(sceneDelegate).refineLevel;

                if (subdiv != NONE && refineLevel > 0) {
                    geometry()->set("is_subd", true);
                    geometry()->set("subd_scheme", subdiv==CATCLARK ? SUBD_SCHEME_CATCLARK : SUBD_SCHEME_BILINEAR);

                    if (not hasPrimvar(mesh_resolutionToken)) {
                        float mesh_resolution = 1 << refineLevel; // default matches Storm
                        pxr::VtValue v = get(mesh_resolutionToken, sceneDelegate);
                        if (not v.IsEmpty()) mesh_resolution = v.Get<float>();
                        geometry()->set("mesh_resolution", mesh_resolution);
                    }

                    if (not hasPrimvar(adaptive_errorToken)) {
                        // this value is provided but it seems to be global to all meshes:
                        float adaptive_error = topology.IsEnabledAdaptive() ? 1.0f : 0.0f;
                        pxr::VtValue v = get(adaptive_errorToken, sceneDelegate);
                        if (not v.IsEmpty()) adaptive_error = v.Get<float>();
                        geometry()->set("adaptive_error", adaptive_error);
                    }

                } else {
                    geometry()->set("is_subd", false);
                    if (not hasPrimvar(smooth_normalToken)) {
                        // RdlMeshGeometry defaults smooth_normal to true. This is changed here to match USD
                        bool smooth_normal = (subdiv == CATCLARK);
                        pxr::VtValue v = get(smooth_normalToken, sceneDelegate);
                        if (not v.IsEmpty()) smooth_normal = v.Get<bool>();
                        geometry()->set("smooth_normal", smooth_normal);
                    }
                }
            }
        }

        // Subd //////////////////////////////////////////////////////////////

        if (pxr::HdChangeTracker::IsSubdivTagsDirty(*dirtyBits, id)) {
            const auto& tags(sceneDelegate->GetSubdivTags(id));
            pxr::TfToken t;
            int n;
            pxr::VtIntArray vi;
            pxr::VtFloatArray vf;

            t = tags.GetVertexInterpolationRule();
            if (t == pxr::PxOsdOpenSubdivTokens->none) n = SUBD_BOUNDARY_NONE;
            else if (t == pxr::PxOsdOpenSubdivTokens->edgeOnly) n = SUBD_BOUNDARY_EDGE_ONLY;
            else n = SUBD_BOUNDARY_EDGE_AND_CORNER;
            geometry()->set("subd_boundary", n);

            t = tags.GetFaceVaryingInterpolationRule();
            if (t == pxr::PxOsdOpenSubdivTokens->none) n = SUBD_FVAR_LINEAR_NONE;
            else if (t == pxr::PxOsdOpenSubdivTokens->cornersOnly) n = SUBD_FVAR_LINEAR_CORNERS_ONLY;
            else if (t == pxr::PxOsdOpenSubdivTokens->cornersPlus1) n = SUBD_FVAR_LINEAR_CORNERS_PLUS1;
            else if (t == pxr::PxOsdOpenSubdivTokens->cornersPlus2) n = SUBD_FVAR_LINEAR_CORNERS_PLUS2;
            else if (t == pxr::PxOsdOpenSubdivTokens->boundaries) n =  SUBD_FVAR_LINEAR_BOUNDARIES;
            else if (t == pxr::PxOsdOpenSubdivTokens->all) n = SUBD_FVAR_LINEAR_ALL;
            else n = SUBD_FVAR_LINEAR_CORNERS_ONLY;
            geometry()->set("subd_fvar_linear", n);

            // t = tags.GetCreaseMethod();
            // creaseMethod is in the API but not supported by either UsdGeometry or RdlMesh at present

            // t = tags.GetTriangleSubdivision();
            // "smooth" or "catmullClark", but RdlMesh doesn't support smooth subdivision method

            vi = tags.GetCreaseIndices();
            if (not vi.empty()) {
                // Hydra has connected strings of edges. Convert to RdlMesh individual edges
                pxr::VtIntArray lengths(tags.GetCreaseLengths());
                const size_t edges = vi.size() - lengths.size();
                vf = tags.GetCreaseWeights();
                bool weightPerEdge = vf.size() >= edges;
                scene_rdl2::rdl2::IntVector result(2 * edges);
                scene_rdl2::rdl2::FloatVector weights(edges);
                size_t i = 0; // index into crease indices
                size_t edge = 0; // index into result
                for (size_t crease = 0; crease < lengths.size(); ++crease) {
                    int n = lengths[crease];
                    for (int k = 0; k + 1 < n; k++) {
                        result[2*edge] = vi[i];
                        result[2*edge + 1] = vi[i + 1];
                        weights[edge] = weightPerEdge ? vf[edge] : vf[crease];
                        ++i;
                        ++edge;
                    }
                    ++i; // don't connect last point with first of next edge
                }
                geometry()->set("subd_crease_indices", result);
                geometry()->set("subd_crease_sharpnesses", weights);
            } else {
                // doing this leaves it in the rdla data
                // geometry()->resetToDefault("subd_crease_indices");
                // geometry()->resetToDefault("subd_crease_sharpnesses");
            }

            vi = tags.GetCornerIndices();
            if (not vi.empty()) {
                geometry()->set("subd_corner_indices", scene_rdl2::rdl2::IntVector(&vi[0], &vi[0] + vi.size()));
                vf = tags.GetCornerWeights();
                geometry()->set("subd_corner_sharpnesses", scene_rdl2::rdl2::FloatVector(&vf[0], &vf[0] + vf.size()));
            } else {
                // geometry()->resetToDefault("subd_corner_indices");
                // geometry()->resetToDefault("subd_corner_sharpnesses");
            }
        }

        // Primvars //////////////////////////////////////////////////////////////

        for (auto& i : dirtyPrimvars) {
            pxr::TfToken name = i.first;
            pxr::VtValue& value = i.second.value;

            if (!updateCommonPrimvars(sceneDelegate, id, name, value, geometry())) {
                if (name == pxr::HdTokens->normals || name == normalToken) {
                    const pxr::VtVec3fArray& v = value.Get<pxr::VtVec3fArray>();
                    const scene_rdl2::rdl2::Vec3f* p = reinterpret_cast<const scene_rdl2::rdl2::Vec3f*>(&v[0]);
                    scene_rdl2::rdl2::Vec3fVector out(p, p + v.size());
                    geometry()->set("normal_list", out);
                    value = pxr::VtValue(); // remove it from UserData primvars

                } else if (name == stToken || name == uvToken) {
                    scene_rdl2::rdl2::Vec2fVector out;
                    if (value.IsHolding<pxr::VtVec3fArray>()) {
                        const pxr::VtVec3fArray& v = value.UncheckedGet<pxr::VtVec3fArray>();
                        out.reserve(v.size());
                        for (const auto& v3 : v)
                            out.emplace_back(*reinterpret_cast<const scene_rdl2::rdl2::Vec2f*>(&v3));
                    } else {
                        const pxr::VtVec2fArray& v = value.Get<pxr::VtVec2fArray>();
                        const scene_rdl2::rdl2::Vec2f* p = reinterpret_cast<const scene_rdl2::rdl2::Vec2f*>(&v[0]);
                        out.assign(p, p + v.size());
                    }
                    geometry()->set("uv_list", std::move(out));
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

    // Clean all dirty bits. If they are not turned off then there will not be another
    // sync() call when they are turned on. Possibly you could leave some on if you cannot
    // update due to them until another dirty bit is also turned on.
    *dirtyBits &= ~pxr::HdChangeTracker::AllSceneDirtyBits;
}

}
