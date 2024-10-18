// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/imaging/hd/rprim.h>
#include <pxr/base/gf/matrix4f.h>

namespace scene_rdl2 {namespace rdl2 {
    class Geometry;
    class UserData;
} }

namespace hdMoonray {

class RenderDelegate;

// This is a mix-in class providing shared code for all Geometry prims
//
// syncAll provides the basic sync implementation for all geometry:
// - Handles transform and double-sided sync from the sceneDelegate to RDL attrs
// - Syncs the "points" primvar to the "vertex_list"/"velocity_list" etc RDL attrs
// - Handles "Override" primvars, of the form "moonray:xyz"  -- setting the RDL attribute
//   "xyz" directly on the RDL object, overriding any indirect setting
// - Other primvars are translated into RDL UserData objects. As a special case, "primId"
//   generates a UserData, even though it is not a primvar
// - Performs material and light assignment and instancing
//
// To sync additional sceneDelegate properties (e.g. topology, display style) to RDL, 
// override "syncAttributes". Call GeometryMixin::syncAttributes() at the end of the 
// overridden version to handle the common properties (transform, double-sided). 
// syncAttributes is called _after_ primvar overrides have been applied : if an attribute
// should be overridable by a "moonray:xyz" primvar, check by calling "isPrimvarUsed" and 
// don't set the value if this returns true, or you will stomp on the override.
//
// To handle additional cases where primvars drive RDL attributes directly (e.g. st/uv and normals)
// override "primvarChanged". If a primvar is not handled by this code, call 
// GeometryMixin::primvarChanged() from the overridden version to handle the standard cases.
//
// See Mesh and BasisCurves for typical subclass examples

class GeometryMixin
{
public:
    GeometryMixin(pxr::HdRprim* p): rprim(*p) {}

    const pxr::SdfPath& getId() const { return rprim.GetId(); }

    // Return geometry if it has already been created
    scene_rdl2::rdl2::Geometry* geometry() const { return mGeometry; }

    bool isPrimvarUsed(const pxr::TfToken& name) { return mAppliedPrimvars.count(name) > 0; }

protected:

    // Creates geometry if needed and performs sync.
    // The subfunctions below should be overridden to customize behavior.
    // clears dirtyBits
    void syncAll(const std::string& className,
                 pxr::HdSceneDelegate *sceneDelegate, RenderDelegate&  renderDelegate,
                 pxr::HdDirtyBits *dirtyBits, pxr::TfToken const &reprToken);

    // Sync normal attributes (not primvars). Check for an overriding primvar before
    // setting a value. The default impl syncs transform and double-sided
    virtual void syncAttributes(pxr::HdSceneDelegate* sceneDelegate, RenderDelegate& renderDelegate,
                                pxr::HdDirtyBits* dirtyBits, const pxr::TfToken& reprToken);

    // Sync a given primvar. If value is empty, the primvar was removed.
    // The default impl syncs 
    // - points/velocities/accelerations, 
    // - overrides ("moonray:..." primvars)
    // - regular primvars (without the "moonray:" prefix) produce userData
    virtual void primvarChanged(pxr::HdSceneDelegate *sceneDelegate, RenderDelegate& renderDelegate,
                                const pxr::TfToken& name,  const pxr::VtValue& value,
                                const pxr::HdInterpolation& interp, const pxr::TfToken& role);

    // return true if this is a volume (affects default material assignment)
    virtual bool isVolume() { return false; }
    // return true if this geometry supports UserData
    virtual bool supportsUserData() { return true; }

    // Subclass must fill these in if there are any parts.
    std::vector<std::string> partList;
    std::vector<pxr::SdfPath> partPaths;
    std::vector<pxr::SdfPath> partMaterials;

    // in principle deletes the current RDL geom object (mGeometry) and sets the ptr to null.
    // in practice the RDL API doesn't support object deletion, and so the object is hidden
    void resetGeometryObject(RenderDelegate&);
    
    bool isMirror() const { return mMirror; }

    // force the object to be invisible, regardless of "primvar:moonray:visible_xyz"
    // settings
    void forceInvisible();

    // restore visibility to what it should be, taking account of "primvar:moonray:visible_xyz"
    // settings. Has no effect if forceInvisible is not in effect.
    void restoreVisibility(pxr::HdSceneDelegate* sceneDelegate);

private:
    friend class Mesh; // for geometryForMeshLight()

    // create the geometry. Returns false if geometry is not created, in which case
    // sync should not proceed
    bool createGeometry(RenderDelegate& renderDelegate, const std::string& className);

    // process and sync all primvars. calls primvarChanged for every changed primvar
    void syncPrimvars(pxr::HdSceneDelegate *sceneDelegate,
                      RenderDelegate& renderDelegate,
                      pxr::HdDirtyBits *dirtyBits);

    // syncs an overriding primvar ("moonray:...")
    void primvarAttributeOverride(const std::string& name, const pxr::VtValue& value);

    // creates/updates userData for a primvar
    void primvarUserData(RenderDelegate& renderDelegate,
                         const pxr::TfToken& name, const pxr::VtValue& value,
                         const pxr::HdInterpolation& interp, const pxr::TfToken& role);

    // set a Vec3f attribute from a primvar with possible motion blur
    void setVec3fPrimvarMb(pxr::HdSceneDelegate* sceneDelegate,
                           const pxr::TfToken& hydraName, const pxr::VtValue& value,
                           const std::string& rdlName_0, const std::string& rdlName_1);

    // perform material and light assignment and instance creation
    void assign(pxr::HdSceneDelegate* sceneDelegate, RenderDelegate& renderDelegate, 
                pxr::HdDirtyBits* dirtyBits);

    // add/update additional UserData, not derived from a primvar
    void addUserData(pxr::TfToken key, scene_rdl2::rdl2::UserData* userData);
    
    // update the primitive_attributes list of user data (if defined)
    void syncPrimitiveAttributes();

    // rprim that this is mixed into
    pxr::HdRprim& rprim;

    // RDL geometry object generated for the rprim
    scene_rdl2::rdl2::Geometry* mGeometry = nullptr;

    // names of the primvars used by this object at last sync. We need this
    // to detect whether a primvar was removed.
    std::set<pxr::TfToken> mAppliedPrimvars;

    // All UserData objects associated with this geometry 
    std::map<pxr::TfToken, scene_rdl2::rdl2::UserData*> mUserData;

    bool mMirror = false; // true if xform is a reflection
    bool mUserDataChanged = false;
    bool mForcedInvisible = false;

    GeometryMixin(const GeometryMixin&)             = delete;
    GeometryMixin &operator =(const GeometryMixin&) = delete;
};

}
