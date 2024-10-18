// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

// Contains primvar-related functions on GeometryMixin, split
// out into this file as they are the most complex part of the code

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

    // filter primvars by name, to remove excessive "junk" primvars
    // return true if primvar should be ignored
    bool primvarFilter(const TfToken& name)
    {
        if (name.GetText()[0] == '_') return true; // Hydra internal variables
        if (not strncmp(name.GetText(), "usd", 3)) return true; // sceneflow data
        if (not strncmp(name.GetText(), "part:", 4)) return true; // obsolete part api
        static const std::set<TfToken> names {
            TfToken("PreMenvPosingRefPose"), // I think this may be something from Pixar's animation
            TfToken("gprimID"), // Hydra sets this but never requests it
            TfToken("asset_name"), // bookeeping from Pixar
            TfToken("orig_vert_index") // often incorrect primvar from downrez code
        };
        if (names.count(name)) return true;
        return false;
    }

    // utility to set a Vec3f attribute
    inline void setVec3fPrimvar(Geometry* geometry,
                                const std::string& rdlName,
                                const VtValue& values)
    {
        if (values.IsHolding<VtVec3fArray>()) {
            const VtVec3fArray& va = values.UncheckedGet<VtVec3fArray>();
            const Vec3f* p = reinterpret_cast<const Vec3f*>(&va[0]);
            geometry->set(rdlName, Vec3fVector(p, p + va.size()));
        } else {
            Logger::warn(rdlName, " requires a Vec3f array");
        }
    }
}

namespace hdMoonray {

void 
GeometryMixin::syncPrimvars(HdSceneDelegate *sceneDelegate,
                            RenderDelegate& renderDelegate,
                            HdDirtyBits     *dirtyBits)
{
    // This function determines which primvars have changed since the last sync
    // and makes a call to primvarChanged() for each one, providing the new value. 
    // Where a primvar was removed, primvarChanged() is called with an empty value
    //
    // mGeometry should be non-null and have an active UpdateGuard
    const SdfPath& id = rprim.GetId();

    // primId is not a primvar, but we treat it as one because it creates an
    // RDL UserData object
    if (HdChangeTracker::IsPrimIdDirty(*dirtyBits, id)) {
        static TfToken primId("primId");
        // for the float cast, see comment at the start of setUserDataValues
        primvarChanged(sceneDelegate, renderDelegate,
                       primId, VtValue(static_cast<float>(rprim.GetPrimId())),
                       HdInterpolation::HdInterpolationConstant,
                       TfToken());
    }
    
    // We will iterate through every primvar, calling primvarChanged if the primvar
    // is dirty. To detect removed primvars, we maintain the list mAppliedPrimvars of
    // all currently applied primvars, and build a list 'removedPrimvars' by comparing
    // the new and old lists.
    std::set<TfToken> removedPrimvars;
    std::swap(mAppliedPrimvars, removedPrimvars);
    mAppliedPrimvars.clear();
    // initially, removedPrimvars has the old list of applied primvars : as we 
    // see current primvars, we will erase them from the removed list

    // process external computations first, so that they have priority.
    // Values for these are fetched in a single call, so we need two loops
    HdExtComputationPrimvarDescriptorVector dirtyCompPrimvars;
    for (size_t i = 0; i < HdInterpolationCount; ++i) {
        HdInterpolation interp = static_cast<HdInterpolation>(i);
        HdExtComputationPrimvarDescriptorVector compPrimvars =
                sceneDelegate->GetExtComputationPrimvarDescriptors(id, interp);
        for (auto const& pv: compPrimvars) {
            if (primvarFilter(pv.name)) continue;
            mAppliedPrimvars.insert(pv.name);
            removedPrimvars.erase(pv.name);
            if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pv.name)) {
                dirtyCompPrimvars.emplace_back(pv);
            }
        }
    }
    // actually compute and update the dirty primvars we discovered
    if (not dirtyCompPrimvars.empty()) {
        HdExtComputationUtils::ValueStore valueStore =
            HdExtComputationUtils::GetComputedPrimvarValues(dirtyCompPrimvars, sceneDelegate);
        for (auto const& compPrimvar : dirtyCompPrimvars) {
            auto const computedValue = valueStore.find(compPrimvar.name);
            primvarChanged(sceneDelegate,
                           renderDelegate,
                           compPrimvar.name,
                           computedValue->second,
                           compPrimvar.interpolation,
                           compPrimvar.role);
        }
    }

    // now process regular primvars, skipping any already seen as computed primvars
    for (size_t i = 0; i < HdInterpolationCount; ++i) {
        HdInterpolation interp = static_cast<HdInterpolation>(i);
        for (HdPrimvarDescriptor const& pv : rprim.GetPrimvarDescriptors(sceneDelegate, interp)) {
            if (not isPrimvarUsed(pv.name) && not primvarFilter(pv.name)) {
                mAppliedPrimvars.insert(pv.name);
                removedPrimvars.erase(pv.name);
                if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pv.name)) {
                    primvarChanged(sceneDelegate,
                                   renderDelegate,
                                   pv.name, 
                                   rprim.GetPrimvar(sceneDelegate, pv.name),
                                   interp, pv.role);
                }
            }
        }
    }

    // finally, process the removed primvars left behind. We will call primvarChanged
    // with an empty VtValue
    for (auto&& name : removedPrimvars) {
        primvarChanged(sceneDelegate, renderDelegate, name, VtValue(),
                       HdInterpolation::HdInterpolationConstant, TfToken());
    }
}

void 
GeometryMixin::primvarChanged(HdSceneDelegate *sceneDelegate,
                              RenderDelegate& renderDelegate,
                              const TfToken& name, 
                              const VtValue& value,
                              const HdInterpolation& interp,
                              const TfToken& role)
{
    // process a changed primvar. If the primvar was deleted, value will be empty
    // should be overridden by subclasses to handle their own specific primvars.
    
    // point primvars are supported by all geometry, so handle them here.
    if (name == HdTokens->points) {
        setVec3fPrimvarMb(sceneDelegate, HdTokens->points, value,
                        "vertex_list_0", "vertex_list_1");
        return;
    } else if (name == HdTokens->velocities) {
        setVec3fPrimvarMb(sceneDelegate, HdTokens->velocities, value,
                         "velocity_list_0", "velocity_list_1");
        return;
    } else if (name == HdTokens->accelerations) {
        // only 1 sample for acceleration
        try {
            if (value.IsEmpty()) {
                mGeometry->resetToDefault("accleration_list"); // sic
            } else {
                setVec3fPrimvar(mGeometry, "accleration_list", value);
            }
        } catch (std::exception& e) {
            // geometry isn't guarantred to have "accleration_list"
        }
        return;
    }
    
    // primvars starting with 'moonray:' act to override an RDL object attribute of the same name
    if (not strncmp(name.GetText(), "moonray:", 8)) {
        primvarAttributeOverride(name.GetString().substr(8), value);
        return;
    }

    // otherwise, primvar generates a corresponding UserData object
    if (supportsUserData()) {
        primvarUserData(renderDelegate, name, value, interp, role);
    } 
}

void
GeometryMixin::primvarAttributeOverride(const std::string& name, const VtValue& value)
{
    // override an RDL attribute value with the given value, or remove an override if
    // value is empty. Sync may not be perfect here, because we would need to force
    // a sync with the appropriate dirty flags to make sure that the unoverridden value
    // is correct, but we don't know what those flags are.
    try { 
        const Attribute* attribute(mGeometry->getSceneClass().getAttribute(name));
        if (value.IsEmpty()) {
            ValueConverter::setDefault(mGeometry, attribute);
        } else {
            ValueConverter::setAttribute(mGeometry, attribute, value);
        }    
    } catch (const std::exception& e) {
        // name wasn't a valid RDL attribute
        Logger::error(rprim.GetId(), ": ", e.what());
    }
}

namespace {
// helper functions for setting up UserData objects

void setUserDataInterpolation(UserData* userData,
                              const HdInterpolation& interp)
{
    switch (interp) {
        case HdInterpolation::HdInterpolationConstant:
            userData->setRate(UserData::Rate::CONSTANT);
            break;
        case HdInterpolation::HdInterpolationUniform:
            // mesh: per-face, curve: per-curve, instancer: constant
            userData->setRate(UserData::Rate::UNIFORM);
            break;
        case HdInterpolation::HdInterpolationVarying:
            // mesh: per-point (linear), curve: per-segment, instancer: per-instance
            userData->setRate(UserData::Rate::VARYING);
            break;
        case HdInterpolation::HdInterpolationVertex:
            // mesh: per-point (subd), curve: per-point, instancer: per-instance
            userData->setRate(UserData::Rate::VERTEX);
            break;
        case HdInterpolation::HdInterpolationFaceVarying:
            // mesh: per-vertex, curve: none, instancer: per-instance
            userData->setRate(UserData::Rate::FACE_VARYING);
            break;
        default:
            userData->setRate(UserData::Rate::AUTO);
    }
}

void setUserDataValues(UserData* userData, 
                       const TfToken& name, 
                       const VtValue& values, 
                       const TfToken& role)
{
    // we have to handle the types case-by-case
    //
    // Apparently, int user data cannot be output into RenderOutput
    // buffers, so id-type primvars must be of float type. If the
    // RenderBuffer is requested as an int format, it is translated
    // from float to int in RenderBuffer::Resolve() [q.v.]
    if (values.IsHolding<VtFloatArray>()) {
        const VtFloatArray& v = values.UncheckedGet<VtFloatArray>();
        const float* p = &v[0];
        userData->setFloatData(name, FloatVector(p, p + v.size()));
    } else if (values.IsHolding<float>()) {
        float v = values.UncheckedGet<float>();
        userData->setFloatData(name, FloatVector{v});
    } else if (values.IsHolding<double>()) {
        float v = float(values.UncheckedGet<double>());
        userData->setFloatData(name, FloatVector{v});
    } else if (values.IsHolding<VtVec2fArray>()) {
        const VtVec2fArray& v = values.UncheckedGet<VtVec2fArray>();
        const Vec2f* p = reinterpret_cast<const Vec2f*>(&v[0]);
        userData->setVec2fData(name, Vec2fVector(p, p + v.size()));
    } else if (values.IsHolding<VtVec3fArray>()) {
        const VtVec3fArray& v = values.UncheckedGet<VtVec3fArray>();
        if (role == HdPrimvarRoleTokens->color) {
            const Rgb* p = reinterpret_cast<const Rgb*>(&v[0]);
            userData->setColorData(name, RgbVector(p, p + v.size()));
        } else {
            const Vec3f* p = reinterpret_cast<const Vec3f*>(&v[0]);
            userData->setVec3fData(name, Vec3fVector(p, p + v.size()));
        }
    } else if (values.IsHolding<GfVec3f>()) {
        const GfVec3f& v = values.UncheckedGet<GfVec3f>();
        if (role == HdPrimvarRoleTokens->color) {
            userData->setColorData(name, RgbVector{reinterpret_cast<const Rgb&>(v)});
        } else {
            userData->setVec3fData(name, Vec3fVector{reinterpret_cast<const Vec3f&>(v)});
        }
    } else if (values.IsHolding<VtStringArray>()) {
        const VtStringArray& v = values.UncheckedGet<VtStringArray>();
        const std::string* p = &v[0];
        userData->setStringData(name, StringVector(p, p + v.size()));
    } else if (values.IsHolding<std::string>()) {
        const std::string& v = values.Get<std::string>();
        userData->setStringData(name, StringVector{v});
    } else if (values.IsHolding<VtUIntArray>()) {
        // HDM-266 moonray does not support attribute type Int for face varying attribute, 
        // cast to float
        const VtUIntArray& v = values.UncheckedGet<VtUIntArray>();
        const float* p = reinterpret_cast<const float*>(&v[0]);
        userData->setFloatData(name, FloatVector(p, p + v.size()));
    } else if (values.IsHolding<VtIntArray>()) {
        const VtIntArray& v = values.UncheckedGet<VtIntArray>();
        // HDM-266 moonray does not support attribute type Int for face varying attribute, 
        // cast to float
        const float* p = reinterpret_cast<const float*>(&v[0]);
        userData->setFloatData(name.GetString(), FloatVector(p, p + v.size()));
    } else if (values.IsHolding<int>()) {
        int v = values.UncheckedGet<int>();
        userData->setIntData(name, IntVector{v});
    } else if (values.IsHolding<long>()) {
        int v = int(values.UncheckedGet<long>());
        userData->setIntData(name, IntVector{v});
    } else if (values.IsHolding<VtBoolArray>()) {
        const VtBoolArray& v = values.UncheckedGet<VtBoolArray>();
        const bool* p = &v[0];
        userData->setBoolData(name, BoolVector(p, p + v.size()));
    } else {
        Logger::warn(userData->getName(), ": ", values.GetTypeName(), " not translated");
        }
    }
}

} // namespace {

namespace hdMoonray {


void
GeometryMixin::primvarUserData(RenderDelegate& renderDelegate,
                               const TfToken& name,
                               const VtValue& value,
                               const HdInterpolation& interp,
                               const TfToken& role)
{
    // a primvar presumed to represent user data has been added, changed or removed

    if (value.IsEmpty()) {
        // indicates primvar was removed, so we should remove user data
       mUserData.erase(name);
       // cannot delete actual RDL object...
       mUserDataChanged = true;
       return;
    }

    // create UserData object if it doesn't already exist
    auto& userData = mUserData[name];
    if (!userData) {
        std::string childName = geometry()->getName() + ".primvars:" + name.GetString();
        SceneObject* object = renderDelegate.createSceneObject("UserData", childName);
        if (!object) return;
        userData = object->asA<UserData>();
        mUserDataChanged = true;
    }

    // store the primvar values into the UserData object
    UpdateGuard guard(renderDelegate, userData);
    setUserDataInterpolation(userData, interp);
    setUserDataValues(userData, name, value, role);
}


// set a Vec3f attribute from a primvar, with motion blur samples
// if present. Resets attribute to default if value is empty
void 
GeometryMixin::setVec3fPrimvarMb(HdSceneDelegate* sceneDelegate,
                                 const TfToken& hydraName,
                                 const VtValue& value,
                                 const std::string& rdlName_0,
                                 const std::string& rdlName_1)
{
    try {
        if (value.IsEmpty()) {
            // primvar was deleted
            mGeometry->resetToDefault(rdlName_0);
            mGeometry->resetToDefault(rdlName_1);
            return;
        }
   
        HdTimeSampleArray<VtValue, 4> vals;
        sceneDelegate->SamplePrimvar(rprim.GetId(), hydraName, &vals);

        if (vals.count <= 1) {
            setVec3fPrimvar(mGeometry, rdlName_0, value);
            mGeometry->resetToDefault(rdlName_1);
        } else {
            const bool sizesMatch =
                vals.values[0].Get<pxr::VtVec3fArray>().size() ==
                vals.values[vals.count - 1].Get<pxr::VtVec3fArray>().size();

            if (sizesMatch) {
                setVec3fPrimvar(mGeometry, rdlName_0, vals.values[0]);
                setVec3fPrimvar(mGeometry, rdlName_1, vals.values[vals.count - 1]);
                                 
            } else {
                // If the sizes of the arrays don't match, then the topology
                // is changing and we should just use the second set of values
                setVec3fPrimvar(mGeometry, rdlName_0, vals.values[vals.count - 1]);
            }
        }          
    } catch (std::exception& e) {
        // may be called in cases that rdlName_0 or _1 don't exist : e.g.
        // if Hydra generates a "points" primvar for procedurals that don't
        // have points
    }
}

}



