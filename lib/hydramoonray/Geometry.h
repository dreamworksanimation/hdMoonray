// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <pxr/imaging/hd/rprim.h>
#include <pxr/base/gf/matrix4f.h>
#include <scene_rdl2/scene/rdl2/Geometry.h>

// This macro is used for the instanceId argument to constructors which was removed in usd-21.2
#if PXR_VERSION < 2102
# define INSTANCERID(x) ,x
#else
# define INSTANCERID(x)
#endif

namespace scene_rdl2 {namespace rdl2 {
class Geometry;
class UserData;
} }

namespace hdMoonray {

class RenderDelegate;

// How to decode pxr::HdInterpolation*:
//     Constant=0, Uniform, Varying, Vertex, Facevarying, Instance, Count;
// Meaning for Mesh:
//     constant, per-face, per-point, per-point, per-vertex, 0;
//     (Varying is linear interpolation, Vertex is subdivision interpolation)
// Meaning for BasisCurves:
//     constant, per-curve, per-segment, per-point, 0, 0;
// Meaning for Instancer:
//     constant, constant, instance, instance, instance, instance;

/// This is a mix-in class providing shared code needed for all rdl2 Geometry objects
/// A base class is not used due to Hydra's unfortunate splitting of this into
/// several classes. See Mesh or BasisCurves for details on how to call this.
class Geometry
{
public:
    Geometry(pxr::HdRprim* p): rprim(*p) {}

    const pxr::SdfPath& getId() const { return rprim.GetId(); }

    /// subclass must provide rdl2 class name
    virtual const std::string& className(pxr::HdSceneDelegate* sceneDelegate) const = 0;

    // Return geometry if it has already been created
    scene_rdl2::rdl2::Geometry* geometry() const { return mGeometry; }

    struct DirtyPrimvar { pxr::VtValue value; pxr::HdInterpolation interpolation; pxr::TfToken role; };
    typedef std::map<pxr::TfToken, DirtyPrimvar> DirtyPrimvars;
    // _UpdateVisibility() and this must be called before createGeometry
    DirtyPrimvars getDirtyPrimvars(pxr::HdSceneDelegate*, RenderDelegate&, pxr::HdDirtyBits*);
    // Call from Sync() method to create the object
    scene_rdl2::rdl2::Geometry* syncCreateGeometry(
        pxr::HdSceneDelegate*, RenderDelegate&, pxr::HdDirtyBits*, const pxr::TfToken& reprToken);
    // Call from other code to create the object, finishing the Sync()
    scene_rdl2::rdl2::Geometry* createGeometry(
        pxr::HdSceneDelegate*, RenderDelegate&);
    // Find by name and create the object
    static scene_rdl2::rdl2::Geometry* createGeometry(
        pxr::HdSceneDelegate*, RenderDelegate&, const pxr::SdfPath&);

    void finalize(RenderDelegate&); // called by RPrim::Finalize()

    void setCommonAttributes( // call after beginUpdate
        pxr::HdSceneDelegate*, RenderDelegate&,
        pxr::HdDirtyBits*, DirtyPrimvars&);

    void setPruned(bool v) {mPruned = v;}
    bool isMirror() const { return mMirror; } // set by setCommonAttributes

    void updatePrimvars(DirtyPrimvars&, RenderDelegate&, bool userDataSetChanged = false);
    bool hasPrimvar(const pxr::TfToken& name) const { return mUserData.count(name) || mMoonrayPrimvars.count(name); }

    void assign(pxr::HdSceneDelegate*, RenderDelegate&, pxr::HdDirtyBits*, bool volume = false);

    pxr::VtValue get(const pxr::TfToken& key, pxr::HdSceneDelegate*) const; // arbitrary usd attribute

    static bool updateCommonPrimvars(pxr::HdSceneDelegate *sceneDelegate,
                                     const pxr::SdfPath& id,
                                     const pxr::TfToken& name,
                                     pxr::VtValue& value,
                                     scene_rdl2::rdl2::Geometry* geometry);

    void updateUserData(pxr::TfToken key,
                        scene_rdl2::rdl2::UserData* userData);

protected:
    // Subclass must fill these in if there are any parts. Currenlty only Mesh does this:
    std::vector<std::string> partList;
    std::vector<pxr::SdfPath> partPaths;
    std::vector<pxr::SdfPath> partMaterials;

private:
    pxr::HdRprim& rprim;

    scene_rdl2::rdl2::Geometry* mGeometry = nullptr;
    std::mutex mCreateMutex;
    pxr::HdDirtyBits  mDirtyBits = 0; // saved from last Sync
    pxr::TfToken mReprToken; // saved from last Sync

    std::map<pxr::TfToken, scene_rdl2::rdl2::UserData*> mUserData;
    std::set<pxr::TfToken> mMoonrayPrimvars;
    std::set<pxr::TfToken> mRemovedMoonrayPrimvars;
    bool mMirror = false; // true if xform is a reflection
    bool mVisibleDirty = true;
    bool mAssigned = false; // whether object has been assigned to a layer
    unsigned mVisibleFlags = 0; // bitmap of all the Moonray visibility
    unsigned mVisibleTurnedOffByPrimvar = 0;
    unsigned mPruned = 0; // display option for pruning procedural geometries.
    bool mUserDataSetChanged = false;
    bool getBool(const pxr::TfToken& name, const pxr::VtValue&, bool dflt) const;
    bool consumeMoonrayPrimvar(pxr::HdSceneDelegate*, const pxr::HdPrimvarDescriptor&);

    Geometry(const Geometry&)             = delete;
    Geometry &operator =(const Geometry&) = delete;
};

}
