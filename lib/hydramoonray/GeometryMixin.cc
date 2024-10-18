// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

// Primvar-related functions on GeometryMixin are in Primvars.cc

#include "GeometryMixin.h"
#include "Instancer.h"
#include "RenderDelegate.h"
#include "Material.h"
#include "ValueConverter.h"
#include <pxr/imaging/hd/extComputationUtils.h>
#include <scene_rdl2/scene/rdl2/Geometry.h>
#include <scene_rdl2/scene/rdl2/Layer.h>
#include <scene_rdl2/scene/rdl2/UserData.h>
#include <pxr/base/gf/vec2f.h>
#include <iostream>

using namespace pxr;
using namespace scene_rdl2::rdl2;
using scene_rdl2::logging::Logger;

namespace {
    // primvar used to set side type
    const TfToken primvarMoonraySideType{"moonray:side_type"};
}

namespace hdMoonray {

void
GeometryMixin::resetGeometryObject(RenderDelegate& renderDelegate)
{
    if (mGeometry) {
        // RDL objects cannot be deleted, so hide the object
        // Also used to hide objects to fix Pixar bug 
        //     https://github.com/PixarAnimationStudios/USD/issues/801
        UpdateGuard guard(renderDelegate, mGeometry);
        forceInvisible();
        mGeometry = nullptr;
    }
}

bool
GeometryMixin::createGeometry(RenderDelegate& renderDelegate,
                              const std::string& className)
{
    // create the geometry. Returns true if sync should continue
   
    // if this class is pruned, don't create the object, and
    // hide it if it already exists
    if (renderDelegate.getPruneProcedural(className) ||
        (isVolume() && renderDelegate.getPruneVolume())) {
        resetGeometryObject(renderDelegate);
        return false;
    } 
    
    if (mGeometry) {
        return true;
    }
   
    SceneObject* object = renderDelegate.createSceneObject(className, rprim.GetId());
    // if there was an error this already printed an error message
    if (object) {
        mGeometry = object->asA<Geometry>();
        // since RDL cannot delete objects, we may get back an existing object that
        // is no longer in use. We want to reset all attributes so that sync is correct.
        UpdateGuard guard(renderDelegate, mGeometry);
        mGeometry->resetAllToDefault();
    } 
    return mGeometry != nullptr;
}

void
GeometryMixin::syncAll(const std::string& className,
                       HdSceneDelegate *sceneDelegate,
                       RenderDelegate&  renderDelegate,
                       HdDirtyBits *dirtyBits,
                       TfToken const &reprToken)
{
    // create the RDL object
    if (createGeometry(renderDelegate, className)) {

        // mGeometry must be non-null
        UpdateGuard guard(renderDelegate, mGeometry);

        // primvars may override other means of setting attributes
        // we handle this by syncing primvars first...
        syncPrimvars(sceneDelegate, renderDelegate, dirtyBits);

        // and then checking in the following function if a primvar
        // is set (function isPrimvarUsed)
        syncAttributes(sceneDelegate, renderDelegate, dirtyBits, reprToken);

        // perform material and light assignment, and instancing
        assign(sceneDelegate, renderDelegate, dirtyBits);

        // populate the "primitive_attributes" list with user data
        syncPrimitiveAttributes();
    }

    // clear dirty bits to indicate everything is synced
    *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;
}

void
GeometryMixin::syncAttributes(HdSceneDelegate* sceneDelegate,
                              RenderDelegate& renderDelegate,
                              HdDirtyBits* dirtyBits,
                            const TfToken& reprToken)
{
    // sync plain attributes. Should be called after syncPrimvars, since
    // it uses the list mAppliedPrimvars to avoid overwriting any primvar overrides
    // mGeometry must be non-null and have an active UpdateGuard.
    // This function should be overridden to handle additional attributes in a
    // subclass.

    const SdfPath& id = rprim.GetId();

    // all geometry has a transform, mapped to RDL node_xform. This
    // cannot be overridden by a primvar
    if (HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
        HdTimeSampleArray<GfMatrix4d, 4> sampledXforms;
        sceneDelegate->SampleTransform(id, &sampledXforms);
        // if there's only one sample, it should match the cached value
        if (sampledXforms.count <= 1) {
            mGeometry->set(mGeometry->sNodeXformKey,
                           reinterpret_cast<const Mat4d&>(sampledXforms.values[0]));
            mMirror = sampledXforms.values[0].GetDeterminant() < 0; // workaround for MOONRAY-3512
        } else {
            // first and last samples will be sample interval boundaries
            mGeometry->set(mGeometry->sNodeXformKey,
                           reinterpret_cast<const Mat4d&>(sampledXforms.values[0]));
            mGeometry->set(mGeometry->sNodeXformKey,
                           reinterpret_cast<const Mat4d&>(sampledXforms.values[sampledXforms.count-1]),
                           TIMESTEP_END);
            mMirror = sampledXforms.values[0].GetDeterminant() < 0; // workaround for MOONRAY-3512
       }
    }

    // side_type can be overridden by a primvar, so we have to check this hasn't happened
    if (HdChangeTracker::IsDoubleSidedDirty(*dirtyBits, id)) {
        if (!isPrimvarUsed(primvarMoonraySideType)) {
            int side_type = 1; // force single-sided
            if (renderDelegate.isDoubleSided() || sceneDelegate->GetDoubleSided(id)) {
                side_type = 0; // force two-sided
            }
            mGeometry->set(mGeometry->sSideTypeKey, side_type);
        }
    }
}
void
GeometryMixin::addUserData(pxr::TfToken key,
                           scene_rdl2::rdl2::UserData* userData)
{
    mUserData[key] = userData;
    mUserDataChanged = true;
}

void
GeometryMixin::syncPrimitiveAttributes()
{
    // update the object's user data list, if it has changed and the class has it.
    // this is done near the end, so that other sync code can add user data via the
    // function above (e.g.the code to generate cryptomatte ids, which depends on the topology)
    if (mUserDataChanged && supportsUserData()) {
        SceneObjectVector userDatas;
        userDatas.reserve(mUserData.size());
        for (auto& i : mUserData) {
            userDatas.push_back(i.second);
        }
        mGeometry->set("primitive_attributes", userDatas);
        mUserDataChanged = false;
    }
}

void
GeometryMixin::assign(HdSceneDelegate* sceneDelegate,
                      RenderDelegate& renderDelegate,
                      HdDirtyBits* dirtyBits)
{
    // perform material and light assignment and instance creation

    const SdfPath& id = rprim.GetId();
    const SdfPath& instancerId = rprim.GetInstancerId();
    
    if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
        rprim.SetMaterialId(sceneDelegate->GetMaterialId(id));
    }

    if (*dirtyBits & (HdChangeTracker::DirtyCategories | 
                      HdChangeTracker::DirtyMaterialId |
                      HdChangeTracker::DirtyVisibility)) {
                
        if (rprim.IsVisible()) {

            // make sure RDL visible_xyz flags are set correctly
            restoreVisibility(sceneDelegate);

            // categories associated with the geometry define which light
            // sets are assigned
            VtArray<TfToken> categories;
            if (instancerId.IsEmpty()) {
                categories = sceneDelegate->GetCategories(id);
            } else {
                // if this geometry is instanced, we have to use GetInstanceCategories()
                std::vector<VtArray<TfToken>> instCategories =
                    sceneDelegate->GetInstanceCategories(instancerId);
                // instCategories is a vector of per-instance Category sets. 
                // Currently we can only apply a single set to the entire set of instances, 
                // so we use the first
                if (instCategories.size() == 0) {
                    // point instancer
                    categories = sceneDelegate->GetCategories(instancerId);
                } else {
                    categories = instCategories[0];
                }
            }
        
            // renderDelegate can translate the categories to RDL objects and
            // fill in an RDL LayerAssignment object with the proper light set
            LayerAssignment assignment;
            renderDelegate.updateAssignmentFromCategories(assignment, categories);
        
            // fill in the material assignment
            Material::get(assignment, 
                          rprim.GetMaterialId(), 
                          renderDelegate, 
                          sceneDelegate, 
                          &rprim, 
                          isVolume());

            // add the assignment to the Layer table
            renderDelegate.assign(mGeometry, assignment);

            // repeat for all parts in the part list
            for (size_t i = 0; i < partList.size(); ++i) {
                // when instanced. light linking for parts can only be inherited from the instancer
                if (instancerId.IsEmpty()) {
                    // not instanced : there could be light linking on the GeomSubset
                    // ---
                    // HDM-374 : GetCategories() does not work for GeomSubset paths, at least in 
                    //    HdSceneIndexAdapterSceneDelegate at ver 0.22.5 : in fact the subsets don't seem to have
                    //    an HdSceneIndexPrim at all. So we use the categories of the full mesh:
                    renderDelegate.updateAssignmentFromCategories(assignment, categories);
                    // This is the correct implementation (if it worked...)
                    //renderDelegate.updateAssignmentFromCategories(assignment, sceneDelegate->GetCategories(partPaths[i]));
                }
                Material::get(assignment, 
                              partMaterials[i], 
                              renderDelegate, 
                              sceneDelegate, 
                              &rprim, 
                              isVolume());
                renderDelegate.assign(mGeometry, partList[i], assignment);
            }

        } else {
            forceInvisible();
            renderDelegate.addUnassigned(mGeometry);
        }
    }

    // make instances
    if (HdChangeTracker::IsInstancerDirty(*dirtyBits, id) ||
        HdChangeTracker::IsInstanceIndexDirty(*dirtyBits, id) ||
        HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
        Instancer* instancer = static_cast<Instancer*>(
            sceneDelegate->GetRenderIndex().GetInstancer(instancerId));
        if (instancer) {
            instancer->makeInstanceGeometry(id, mGeometry, this, 0);
        }
    }
   
}
namespace {

    struct AttrPair {const scene_rdl2::rdl2::AttributeKey<bool>& moonrayKey; pxr::TfToken usdKey;};

    const AttrPair visibleAttrs[] = {
        { scene_rdl2::rdl2::Geometry::sVisibleCamera, pxr::TfToken("moonray:visible_in_camera") },
        { scene_rdl2::rdl2::Geometry::sVisibleShadow, pxr::TfToken("moonray:visible_shadow") },
        { scene_rdl2::rdl2::Geometry::sVisibleDiffuseReflection, pxr::TfToken("moonray:visible_diffuse_reflection") },
        { scene_rdl2::rdl2::Geometry::sVisibleDiffuseTransmission, pxr::TfToken("moonray:visible_diffuse_transmission") },
        { scene_rdl2::rdl2::Geometry::sVisibleGlossyReflection, pxr::TfToken("moonray:visible_glossy_reflection") },
        { scene_rdl2::rdl2::Geometry::sVisibleGlossyTransmission, pxr::TfToken("moonray:visible_glossy_transmission") },
        { scene_rdl2::rdl2::Geometry::sVisibleMirrorReflection, pxr::TfToken("moonray:visible_mirror_reflection") },
        { scene_rdl2::rdl2::Geometry::sVisibleMirrorTransmission, pxr::TfToken("moonray:visible_mirror_transmission") },
        { scene_rdl2::rdl2::Geometry::sVisiblePhase, pxr::TfToken("moonray:visible_volume") },
};

}

void
GeometryMixin::forceInvisible()
{
    // make sure the geometry object is invisible in all cases
    for (const auto& i : visibleAttrs) {
        mGeometry->set(i.moonrayKey, false);
    }
    // flag that we did this, to avoid restoring unnecessarily
    mForcedInvisible = true;
}

void
GeometryMixin::restoreVisibility(HdSceneDelegate* sceneDelegate)
{
    // check that we are in a forced invisible state
    if (!mForcedInvisible) return;
    // we have to reread every primvar that affects visibility
    for (const auto& i : visibleAttrs) {
        VtValue val = rprim.GetPrimvar(sceneDelegate, i.usdKey);
        bool vis = true;
        // allow the primvars to be int or bool
        if (val.IsHolding<bool>()) vis = val.UncheckedGet<bool>();
        else if (val.IsHolding<int>()) vis = (val.UncheckedGet<int>() != 0);
        mGeometry->set(i.moonrayKey, vis);
    }
    mForcedInvisible = false;
}

}

