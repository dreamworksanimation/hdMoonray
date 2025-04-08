// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Instancer.h"
#include "RenderDelegate.h"

#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/base/gf/rotation.h>

#include <scene_rdl2/scene/rdl2/Geometry.h>
#include <scene_rdl2/scene/rdl2/Layer.h>
#include <scene_rdl2/scene/rdl2/UserData.h>
#include <scene_rdl2/scene/rdl2/Layer.h>

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/quath.h>

#include <iostream>

namespace hdMoonray {

enum Method {
    XFORM_ATTRIBUTES = 0,
    POINT_FILE = 1,
    XFORM_LIST = 2
};

using scene_rdl2::logging::Logger;

Instancer::Instancer(
    pxr::HdSceneDelegate* delegate,
    const pxr::SdfPath& id):
    HdInstancer(delegate, id)
{ }

Instancer::~Instancer()
{
    // release
}

// Hydra sends several apparent junk primvars that are not in the usd data, ignore them
static bool
ignorePrimvar(pxr::TfToken name)
{
    if (name.GetText()[0] == '_') return true; // Hydra internal variables
    if (not strncmp(name.GetText(), "usd", 3)) return true; // sceneflow data
    static const std::set<pxr::TfToken> names {
        pxr::TfToken("asset_name"), // bookeeping from Pixar
        pxr::TfToken("mesh_path"), // from sprinkles, huge array of repeated strings
        pxr::TfToken("hit_prim_path") // new version of the sprinkles huge array
    };
    if (names.count(name)) return true;
    return false;
}

void
#if PXR_VERSION < 2102
Instancer::sync(pxr::HdSceneDelegate* sceneDelegate,
                pxr::HdDirtyBits* dirtyBits)
#else
Instancer::Sync(pxr::HdSceneDelegate* sceneDelegate,
                pxr::HdRenderParam* renderParam,
                pxr::HdDirtyBits* dirtyBits)
#endif
{
    const pxr::SdfPath& id = GetId();
    //std::cout << id << " sync()\n";

#if PXR_VERSION >= 2102
    _UpdateInstancer(sceneDelegate, dirtyBits);
#endif

    if (pxr::HdChangeTracker::IsInstancerDirty(*dirtyBits, id)) {
        // std::cout << " instancer dirty\n";
        // not clear what, if anything, is covered by this
        // Possibly clear out all the prototype references and it will refill it
    }

    if (pxr::HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
        mXform = sceneDelegate->GetInstancerTransform(id);
        // std::cout << id << " transform = " << mXform << std::endl;
    }

    if (pxr::HdChangeTracker::IsInstanceIndexDirty(*dirtyBits, id)) {
        // std::cout << " instancer index dirty\n";
        // this does not do anything, the indexes are looked up per-prototype below
    }

    if (pxr::HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id)) {
        pxr::TfTokenVector primvarNames;
        const pxr::HdPrimvarDescriptorVector& primvars =
            sceneDelegate->GetPrimvarDescriptors(id, pxr::HdInterpolationInstance);

        for (const pxr::HdPrimvarDescriptor& pv: primvars) {
            if (pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pv.name)) {
                //std::cout << " primvar " << pv.name << " is dirty\n";
                if (ignorePrimvar(pv.name)) continue;
                PrimvarInfo& info = mPrimvars[pv.name];
                info.value = sceneDelegate->Get(id, pv.name);
                info.role = pv.role;
            }
        }
    }

    *dirtyBits = 0;
}


// The SceneDelegate api only allows access to the indices by using the prototypeId, which
// we don't have until this is called. Even the number of indices is unavailable as far
// as I can tell. So this function must update the data for that subset of indices.
// Curently a different InstanceGeometry is created for each prototype. It may be possible
// to use a single one if there is a way to incrementally update the references and refIndices.
void
Instancer::makeInstanceGeometry(const pxr::SdfPath& prototypeId, scene_rdl2::rdl2::Geometry* prototype,
                                GeometryMixin* geometry, size_t level, size_t childCount)
{
    const pxr::SdfPath& id = GetId();
    pxr::HdSceneDelegate* sceneDelegate = GetDelegate();

#if PXR_VERSION < 2102
    pxr::HdChangeTracker& changeTracker = sceneDelegate->GetRenderIndex().GetChangeTracker();
    if (pxr::HdChangeTracker::IsDirty(changeTracker.GetInstancerDirtyBits(id))) {
        std::lock_guard<std::mutex> lock(mMapMutex); // double-check with lock so only one thread calls Sync()
        pxr::HdDirtyBits dirtyBits = changeTracker.GetInstancerDirtyBits(id);
        if (pxr::HdChangeTracker::IsDirty(dirtyBits)) {
            sync(sceneDelegate, &dirtyBits);
            changeTracker.MarkInstancerClean(id);
        }
    }
#else
    _SyncInstancerAndParents(sceneDelegate->GetRenderIndex(), id);
#endif

    RenderDelegate& renderDelegate = *reinterpret_cast<RenderDelegate*>(sceneDelegate->GetRenderIndex().GetRenderDelegate());

    pxr::VtIntArray indices = sceneDelegate->GetInstanceIndices(id, prototypeId);
    const size_t count = indices.size();

    // may want to do something special if 0 or 1 indices?
    //std::cout << id << "::makeInstanceGeometry(" << prototypeId << ") indices.size = " << count << std::endl;

    scene_rdl2::rdl2::Geometry* instancer;
    scene_rdl2::rdl2::UserData* instanceId;
    {   std::lock_guard<std::mutex> lock(mMapMutex);
        scene_rdl2::rdl2::Geometry*& mapEntry = mInstancers[prototype];
        if (not mapEntry) {
            std::string name = prototype->getName() + "/Instancer";
            scene_rdl2::rdl2::SceneObject* object = renderDelegate.createSceneObject("RdlInstancerGeometry", name);
            if (not object) return; // it already printed an error, give up
            mapEntry = object->asA<scene_rdl2::rdl2::Geometry>();
            renderDelegate.assign(mapEntry, scene_rdl2::rdl2::LayerAssignment());
            instanceId = renderDelegate.createSceneObject("UserData", prototype->getName() + "/instanceId")->asA<scene_rdl2::rdl2::UserData>();
        } else {
            instanceId = mapEntry->get<scene_rdl2::rdl2::SceneObjectVector>("primitive_attributes")[0]->asA<scene_rdl2::rdl2::UserData>();
        }
        instancer = mapEntry;
    }

    {   UpdateGuard guard(renderDelegate, instanceId);
        std::string name("instanceId");
        if (level) name.push_back('A'+level-1);
        // std::cout << geometry->getId() << ' ' << name << ' ' << count << 'x' << childCount << std::endl;
        // at the moment you cannot get ints into a RenderOutput so convert to float
        scene_rdl2::rdl2::FloatVector out(count);
        for (size_t i = 0; i < count; ++i)
            out[i] = i * childCount;
        instanceId->setFloatData(name, out);
    }


    // add crytomatte id if enabled
    const std::string m_name("prim_id");
    std::string prim_id = prototype->getName() + "/Instancer.primvars:" + m_name;
    scene_rdl2::rdl2::UserData* cryptoId =
            renderDelegate.createSceneObject("UserData", prim_id)->asA<scene_rdl2::rdl2::UserData>();
    UpdateGuard guard(cryptoId);
    scene_rdl2::rdl2::FloatVector data(1);
    data[0]=MurmurHash3_to_float(instancer->getName().c_str());
    //std::cout << instancer->getName() << ": " << data[0] <<std::endl;
    cryptoId->setFloatData(m_name, data);

    // MOONSHINE-1533: Moonray uses the number of transforms to count instances, while Hydra Prman
    // and Embree use the size of the protoIndices array. For bad data where these are unequal,
    // resize the primvars to match the number of instances, so all the renderers produce the same
    // number of instances.
    scene_rdl2::rdl2::SceneObjectVector primitiveAttributes{instanceId};
    primitiveAttributes.push_back(cryptoId);

    for (auto p : mPrimvars) {
        const pxr::TfToken& name = p.first;
#if PXR_VERSION < 2311
        if (name == pxr::HdInstancerTokens->instanceTransform ||
            name == pxr::HdInstancerTokens->scale ||
            name == pxr::HdInstancerTokens->rotate ||
            name == pxr::HdInstancerTokens->translate)
#else
        if (name == pxr::HdInstancerTokens->instanceTransforms ||
            name == pxr::HdInstancerTokens->instanceScales ||
            name == pxr::HdInstancerTokens->instanceRotations ||
            name == pxr::HdInstancerTokens->instanceTranslations)
#endif
            continue; // skip the ones used directly

        // HDM-130: Moonray InstanceGeometry primvars override primvars on the prototype, which
        // is opposite how USD works. Don't create the primvar if it will override incorrectly.
        if (geometry->isPrimvarUsed(name)) continue;
        // Fixme: this also needs to be done with each intermediate instancer.

        const pxr::VtValue& value = p.second.value;
        const pxr::TfToken& role = p.second.role;
        std::string objname = prototype->getName() + "/Instancer.primvars:" + name.GetString();
        scene_rdl2::rdl2::UserData* primvar =
            renderDelegate.createSceneObject("UserData", objname)->asA<scene_rdl2::rdl2::UserData>();

        UpdateGuard guard(primvar);
        // there are lots of types, just get the ones encountered so far:
        if (value.IsHolding<pxr::VtFloatArray>()) {
            const pxr::VtFloatArray& v = value.UncheckedGet<pxr::VtFloatArray>();
            if (v.empty()) continue; // don't crash on error
            scene_rdl2::rdl2::FloatVector out(count);
            for (size_t i = 0; i < count; ++i) out[i] = v[indices[i] % v.size()];
            primvar->setFloatData(name.GetString(), out);
        } else if (value.IsHolding<pxr::VtVec2fArray>()) {
            const pxr::VtVec2fArray& v = value.UncheckedGet<pxr::VtVec2fArray>();
            if (v.empty()) continue; // don't crash on error
            scene_rdl2::rdl2::Vec2fVector out(count);
            for (size_t i = 0; i < count; ++i) out[i] = reinterpret_cast<const scene_rdl2::rdl2::Vec2f&>(v[indices[i] % v.size()]);
            primvar->setVec2fData(name.GetString(), out);
        } else if (value.IsHolding<pxr::VtVec3fArray>()) {
            const pxr::VtVec3fArray& v = value.UncheckedGet<pxr::VtVec3fArray>();
            if (v.empty()) continue; // don't crash on error
            if (role == pxr::HdPrimvarRoleTokens->color) {
                scene_rdl2::rdl2::RgbVector out(count);
                for (size_t i = 0; i < count; ++i) out[i] = reinterpret_cast<const scene_rdl2::rdl2::Rgb&>(v[indices[i] % v.size()]);
                primvar->setColorData(name.GetString(), out);
            } else {
                scene_rdl2::rdl2::Vec3fVector out(count);
                for (size_t i = 0; i < count; ++i) out[i] = reinterpret_cast<const scene_rdl2::rdl2::Vec3f&>(v[indices[i] % v.size()]);
                primvar->setVec3fData(name.GetString(), out);
            }
        } else if (value.IsHolding<pxr::GfVec3f>()) {
            const pxr::GfVec3f& v = value.UncheckedGet<pxr::GfVec3f>();
            if (role == pxr::HdPrimvarRoleTokens->color) {
                primvar->setColorData(name.GetString(), scene_rdl2::rdl2::RgbVector{reinterpret_cast<const scene_rdl2::rdl2::Rgb&>(v)});
            } else {
                primvar->setVec3fData(name.GetString(), scene_rdl2::rdl2::Vec3fVector{reinterpret_cast<const scene_rdl2::rdl2::Vec3f&>(v)});
            }
        } else if (value.IsHolding<pxr::VtIntArray>()) {
            const pxr::VtIntArray& v = value.UncheckedGet<pxr::VtIntArray>();
            if (v.empty()) continue; // don't crash on error
            scene_rdl2::rdl2::IntVector out(count);
            for (size_t i = 0; i < count; ++i) out[i] = v[indices[i] % v.size()];
            primvar->setIntData(name.GetString(), out);
        } else {
            Logger::warn(primvar->getName(),": ",value.GetTypeName()," not translated");
        }
        primitiveAttributes.push_back(primvar);
    }

    {   UpdateGuard guard(instancer);
        instancer->set("primitive_attributes", primitiveAttributes);

        instancer->set(instancer->sNodeXformKey, reinterpret_cast<const scene_rdl2::rdl2::Mat4d&>(mXform));

        instancer->set("references", scene_rdl2::rdl2::SceneObjectVector{prototype});
        // there is also refIndices but that can be left at default value

        instancer->set("use_reference_xforms", true);

#if PXR_VERSION < 2311
        auto i = mPrimvars.find(pxr::HdInstancerTokens->instanceTransform);
#else
        auto i = mPrimvars.find(pxr::HdInstancerTokens->instanceTransforms);
#endif
        if (i != mPrimvars.end()) {
            instancer->set<int>("method", Method::XFORM_LIST);
            const pxr::VtValue& value = i->second.value;
            const pxr::VtMatrix4dArray& v = value.Get<pxr::VtMatrix4dArray>();
            if (not v.empty()) {
                std::vector<pxr::GfMatrix4d> mv(count);
                for (size_t i = 0; i < count; ++i)
                    mv[i] = v[indices[i] % v.size()];
                instancer->set("xform_list", reinterpret_cast<const std::vector<scene_rdl2::rdl2::Mat4d>&>(mv));
            }

        } else {
            instancer->set<int>("method", Method::XFORM_ATTRIBUTES);

#if PXR_VERSION < 2311
            i = mPrimvars.find(pxr::HdInstancerTokens->scale);
#else
            i = mPrimvars.find(pxr::HdInstancerTokens->instanceScales);
#endif
            if (i != mPrimvars.end()) {
                const pxr::VtValue& value = i->second.value;
                const pxr::VtVec3fArray& v = value.Get<pxr::VtVec3fArray>();
                if (not v.empty()) {
                    std::vector<pxr::GfVec3f> mv(count);
                    for (size_t i = 0; i < count; ++i)
                        mv[i] = v[indices[i] % v.size()];
                    instancer->set("scales", reinterpret_cast<const std::vector<scene_rdl2::rdl2::Vec3f>&>(mv));
                }
            }

#if PXR_VERSION < 2311
            i = mPrimvars.find(pxr::HdInstancerTokens->rotate);
#else
            i = mPrimvars.find(pxr::HdInstancerTokens->instanceRotations);
#endif
            if (i != mPrimvars.end()) {
                const pxr::VtValue& value = i->second.value;

                // in 0.21.11 the type of the rotations attr switched from VtVec4fArray to VtQuathArray
                if (value.IsHolding<pxr::VtQuathArray>()) {
                    const pxr::VtQuathArray& quats = value.Get<pxr::VtQuathArray>();
                    if (not quats.empty()) {
                        std::vector<scene_rdl2::rdl2::Vec4f> vecs(count);
                        for (size_t i = 0; i < count; ++i) {
                            const pxr::GfQuath& quat = quats[indices[i] % quats.size()];
                            vecs[i] =
                                scene_rdl2::rdl2::Vec4f(float(quat.GetImaginary()[0]),
                                                        float(quat.GetImaginary()[1]),
                                                        float(quat.GetImaginary()[2]),
                                                        float(quat.GetReal()));
                        }
                        instancer->set("orientations", vecs);
                    }
                } else {
                    const pxr::VtVec4fArray& v = value.Get<pxr::VtVec4fArray>();
                    if (not v.empty()) {
                        std::vector<pxr::GfVec4f> mv(count);
                        for (size_t i = 0; i < count; ++i) {
                            const pxr::GfVec4f& q = v[indices[i] % v.size()];
                            mv[i] = pxr::GfVec4f(q[1],q[2],q[3],q[0]);
                        }
                        instancer->set("orientations", reinterpret_cast<const std::vector<scene_rdl2::rdl2::Vec4f>&>(mv));
                    }
                }
            }
#if PXR_VERSION < 2311
            i = mPrimvars.find(pxr::HdInstancerTokens->translate);
#else
            i = mPrimvars.find(pxr::HdInstancerTokens->instanceTranslations);
#endif
            if (i != mPrimvars.end()) {
                const pxr::VtValue& value = i->second.value;
                const pxr::VtVec3fArray& v = value.Get<pxr::VtVec3fArray>();
                if (not v.empty()) {
                    std::vector<pxr::GfVec3f> mv(count);
                    for (size_t i = 0; i < count; ++i)
                        mv[i] = v[indices[i] % v.size()];
                    instancer->set("positions", reinterpret_cast<const std::vector<scene_rdl2::rdl2::Vec3f>&>(mv));
                }
            }
        }
    }

    // This instancer may itself be a prototype for another instancer!
    if (not GetParentId().IsEmpty()) {
        Instancer* parent = (Instancer*)sceneDelegate->GetRenderIndex().GetInstancer(GetParentId());
        parent->makeInstanceGeometry(id, instancer, geometry, level+1, count * childCount);
    }

}

}
