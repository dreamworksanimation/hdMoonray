// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Geometry.h"
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

const pxr::TfToken doNotCastShadows("doNotCastShadows");
constexpr unsigned VISIBLE_SHADOW = 1; // index for visible_shadow

void
setMoonrayVec3fPrimvar(scene_rdl2::rdl2::Geometry& geometry,
                       const std::string& name,
                       const pxr::VtVec3fArray& values)
{
    const scene_rdl2::rdl2::Vec3f* p =
        reinterpret_cast<const scene_rdl2::rdl2::Vec3f*>(&values[0]);
    geometry.set(name,
                 scene_rdl2::rdl2::Vec3fVector(p, p + values.size()));
}

void
setMoonrayVec3fPrimvarMb(pxr::HdSceneDelegate& sceneDelegate,
                         scene_rdl2::rdl2::Geometry& geometry,
                         const pxr::SdfPath& id,
                         const pxr::TfToken& hdToken,
                         pxr::VtValue& value,
                         const std::string& name0,
                         const std::string& name1)
{
    pxr::HdTimeSampleArray<pxr::VtValue, 4> vals;
    sceneDelegate.SamplePrimvar(id, hdToken, &vals);

    if (vals.count <= 1) {
        setMoonrayVec3fPrimvar(geometry, name0,
                               value.Get<pxr::VtVec3fArray>());
    } else {
        setMoonrayVec3fPrimvar(geometry, name0,
                               vals.values[0].Get<pxr::VtVec3fArray>());

        setMoonrayVec3fPrimvar(geometry, name1,
                               vals.values[vals.count - 1].Get<pxr::VtVec3fArray>());
    }
}

}

namespace hdMoonray {

using scene_rdl2::logging::Logger;

// Return an arbitrary attribute value off the usd prim
pxr::VtValue
Geometry::get(const pxr::TfToken& key, pxr::HdSceneDelegate* sceneDelegate) const
{
#if PXR_VERSION >= 2011
    return sceneDelegate->Get(rprim.GetId(), key);
#endif
    return sceneDelegate->GetLightParamValue(rprim.GetId(), key);
}

// Also used to hide objects to fix Pixar bug https://github.com/PixarAnimationStudios/USD/issues/801
void
Geometry::finalize(RenderDelegate& renderDelegate)
{
    if (mGeometry && mVisibleFlags && mAssigned) {
        UpdateGuard guard(renderDelegate, geometry());
        for (const auto& i: visibleAttrs) {
            mGeometry->set(i.moonrayKey, false);
        }
        mVisibleDirty = true;
    }
}

// Hydra sends several apparent junk primvars that are not in the usd data, ignore them
// Also ignore some bookeeping primvars that are probalby not used by renders
static bool
ignorePrimvar(pxr::TfToken name)
{
    if (name.GetText()[0] == '_') return true; // Hydra internal variables
    if (not strncmp(name.GetText(), "usd", 3)) return true; // sceneflow data
    if (not strncmp(name.GetText(), "part:", 4)) return true; // obsolete part api
    static const std::set<pxr::TfToken> names {
        pxr::TfToken("PreMenvPosingRefPose"), // I think this may be something from Pixar's animation
        pxr::TfToken("gprimID"), // Hydra sets this but never requests it
        pxr::TfToken("asset_name"), // bookeeping from Pixar
        pxr::TfToken("orig_vert_index") // often incorrect primvar from downrez code
    };
    if (names.count(name)) return true;
    return false;
}


// This also updates the visibility flags (from primvars:moonray:visible_* attributes) so must
// be called before createGeometry. Before calling this, caller must call the protected _UpdateVisibility method first:
// if (pxr::HdChangeTracker::IsVisibilityDirty(*dirtyBits, rprim.GetId()))
//     rprim._UpdateVisibility(sceneDelegate, dirtyBits);
Geometry::DirtyPrimvars
Geometry::getDirtyPrimvars(pxr::HdSceneDelegate* sceneDelegate,
                           RenderDelegate& renderDelegate,
                           pxr::HdDirtyBits* dirtyBits)
{
    const pxr::SdfPath& id = rprim.GetId();
    DirtyPrimvars dirtyPrimvars;

    if (pxr::HdChangeTracker::IsPrimIdDirty(*dirtyBits, id)) {
        static pxr::TfToken primId("primId");
        // at the moment you cannot get ints into a RenderOutput so convert to float
        dirtyPrimvars[primId].value = pxr::VtValue(float(rprim.GetPrimId()));
    }

    // Update directly from the source prim:
    if (pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pxr::HdTokens->normals) ||
        pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pxr::HdTokens->widths) ||
        pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pxr::HdTokens->primvar) ||
        pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pxr::HdTokens->points)) {

        // Run the HdExtComputation objects and build list of dirtied primvars
        pxr::HdExtComputationPrimvarDescriptorVector dirtyCompPrimvars;
        for (size_t i = 0; i < pxr::HdInterpolationCount; ++i) {
            pxr::HdInterpolation interp = static_cast<pxr::HdInterpolation>(i);
            pxr::HdExtComputationPrimvarDescriptorVector compPrimvars =
                sceneDelegate->GetExtComputationPrimvarDescriptors(id, interp);
            for (auto const& pv: compPrimvars) {
                if (pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pv.name) && not ignorePrimvar(pv.name)) {
                    dirtyCompPrimvars.emplace_back(pv);
                }
            }
        }
        if (not dirtyCompPrimvars.empty()) {
            pxr::HdExtComputationUtils::ValueStore valueStore =
                pxr::HdExtComputationUtils::GetComputedPrimvarValues(dirtyCompPrimvars, sceneDelegate);
            for (auto const& compPrimvar : dirtyCompPrimvars) {
                auto const it = valueStore.find(compPrimvar.name);
                auto& p = dirtyPrimvars[compPrimvar.name];
                p.value = it->second;
                p.interpolation = compPrimvar.interpolation;
                p.role = compPrimvar.role;
            }
        }

        mVisibleTurnedOffByPrimvar = 0;
        std::swap(mMoonrayPrimvars, mRemovedMoonrayPrimvars);
        mMoonrayPrimvars.clear();
        for (size_t i = 0; i < pxr::HdInterpolationCount; ++i) {
            pxr::HdInterpolation interp = static_cast<pxr::HdInterpolation>(i);
            for (pxr::HdPrimvarDescriptor const& pv: rprim.GetPrimvarDescriptors(sceneDelegate, interp)) {
                if (consumeMoonrayPrimvar(sceneDelegate, pv)) continue;
                // all others go into primitive_attributes
                if (pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pv.name) && not ignorePrimvar(pv.name)) {
                    auto& p = dirtyPrimvars[pv.name];
                    if (p.value.IsEmpty()) { // don't overwrite extComputation
                        p.value = rprim.GetPrimvar(sceneDelegate, pv.name);
                        p.interpolation = interp;
                        p.role = pv.role;
                    }
                }
            }
        }
    }

    // interpret any moonray:visible_* attributes
    unsigned visibleFlags = 0;
    if (rprim.IsVisible()) {
        unsigned bit = 1;
        for (const auto& i : visibleAttrs) {
            if (not (mVisibleTurnedOffByPrimvar & bit)) {
                pxr::VtValue v = get(i.usdKey, sceneDelegate);
                if (getBool(i.usdKey, v, true))
                    visibleFlags |= bit;
            }
            bit <<= 1;
        }
    }
    if (visibleFlags != mVisibleFlags) {
        mVisibleFlags = visibleFlags;
        mVisibleDirty = true;
    }

    return dirtyPrimvars;
}


bool
Geometry::getBool(const pxr::TfToken& name, const pxr::VtValue& v, bool dflt) const
{
    if (v.IsEmpty()) {
        return dflt;
    } else if (v.IsHolding<bool>()) {
        return v.UncheckedGet<bool>();
    } else if (v.IsHolding<int>()) {
        return v.UncheckedGet<int>() != 0;
    } else {
        Logger::error(rprim.GetId(), '.', name, ": cannot convert ", v.GetTypeName(), " to bool");
        return dflt;
    }
}


// Check if primvar is a moonray: override and record that the override is being done.
// For visiblity store it in mVisibleTurnedOffByPrimvar and return true to stop redundant store of primvar
bool
Geometry::consumeMoonrayPrimvar(
    pxr::HdSceneDelegate* sceneDelegate,
    const pxr::HdPrimvarDescriptor& pv)
{
    if (not strncmp(pv.name.GetText(), "moonray:", 8)) { // must start with "moonray:"
        if (not strncmp(pv.name.GetText()+8, "visible_", 8)) { // starts with "moonray:visible_"
            unsigned bit = 1;
            for (const auto& i : visibleAttrs) {
                if (pv.name == i.usdKey) {
                    if (not getBool(pv.name, rprim.GetPrimvar(sceneDelegate, pv.name), true))
                        mVisibleTurnedOffByPrimvar |= bit;
                    return true;
                }
                bit <<= 1;
            }
        }
        mMoonrayPrimvars.insert(pv.name);
        mRemovedMoonrayPrimvars.erase(pv.name);
    } else {
        if (pv.name == doNotCastShadows) { // interpret primvar set for Nvidia renderer in Attic scene:
            if (getBool(pv.name, rprim.GetPrimvar(sceneDelegate, pv.name), false))
                mVisibleTurnedOffByPrimvar |= (1 << VISIBLE_SHADOW);
            return true;
        }
    }
    return false;
}


scene_rdl2::rdl2::Geometry*
Geometry::syncCreateGeometry(
    pxr::HdSceneDelegate* sceneDelegate,
    RenderDelegate& renderDelegate,
    pxr::HdDirtyBits     *dirtyBits,
    pxr::TfToken const   &reprToken)
{
    if (mGeometry) return mGeometry;
    std::lock_guard<std::mutex> lock(mCreateMutex);
    if (mGeometry) return mGeometry;
    renderDelegate.setSceneDelegate(sceneDelegate);
    // Don't create invisible geometry, but if createGeometry() is called it will call Sync() again
    if (not mVisibleFlags) {
        *dirtyBits &= ~(pxr::HdChangeTracker::DirtyVisibility | pxr::HdChangeTracker::DirtyRenderTag);
        mDirtyBits = *dirtyBits;
        mReprToken = reprToken;
        return nullptr;
    }
    mDirtyBits = 0; // make sure Sync is not called a second time
    scene_rdl2::rdl2::SceneObject* object =
        renderDelegate.createSceneObject(className(sceneDelegate), rprim.GetId());
    // if there was an error this already printed an error message
    mGeometry = object ? object->asA<scene_rdl2::rdl2::Geometry>() : nullptr;
    return mGeometry;
}

// GeometryLight and CoordSys can asynchronously request a piece of geometry, and want
// it even if it is invisible. This creates it if not done yet, and finishes the Sync().
scene_rdl2::rdl2::Geometry*
Geometry::createGeometry(
    pxr::HdSceneDelegate* sceneDelegate,
    RenderDelegate& renderDelegate)
{
    if (mGeometry) return mGeometry;
    std::lock_guard<std::mutex> lock(mCreateMutex);
    if (mGeometry) return mGeometry;

    scene_rdl2::rdl2::SceneObject* object =
        renderDelegate.createSceneObject(className(sceneDelegate), rprim.GetId());
    // if there was an error this already printed an error message
    mGeometry = object ? object->asA<scene_rdl2::rdl2::Geometry>() : nullptr;
    // If Sync was already called, need to call it again to fill in object:
    if (mDirtyBits) {
        if (mGeometry) rprim.Sync(sceneDelegate, renderDelegate.GetRenderParam(), &mDirtyBits, mReprToken);
        mDirtyBits = 0;
    }
    return mGeometry;
}

/* static */ scene_rdl2::rdl2::Geometry*
Geometry::createGeometry(
    pxr::HdSceneDelegate* sceneDelegate,
    RenderDelegate& renderDelegate,
    const pxr::SdfPath& id)
{
    pxr::HdRprim* prim =
        const_cast<pxr::HdRprim*>(sceneDelegate->GetRenderIndex().GetRprim(id));
    Geometry* geometry = dynamic_cast<Geometry*>(prim);
    if (geometry) {
        return geometry->createGeometry(sceneDelegate, renderDelegate);
    }
    return nullptr;
}


// First thing to call after beginUpdate. This sets the visibility flags, the transform,
// the side_type, and any primvars:moonray:x values.
void
Geometry::setCommonAttributes(
    pxr::HdSceneDelegate* sceneDelegate,
    RenderDelegate& renderDelegate,
    pxr::HdDirtyBits* dirtyBits,
    DirtyPrimvars& dirtyPrimvars)
{
    const pxr::SdfPath& id = rprim.GetId();

    if ((mVisibleDirty && (mVisibleFlags || mAssigned)) || pxr::HdChangeTracker::IsVisibilityDirty(*dirtyBits, id))
    {
        unsigned bit = 1;
        for (const auto& i : visibleAttrs) {
            bool visiblity = (mVisibleFlags & bit) != 0;
            // respect the visibility tag set on the usd stage.
            visiblity = visiblity && !mPruned;
            mGeometry->set(i.moonrayKey, visiblity);
            bit <<= 1;
        }
        mVisibleDirty = false;
    }

    if (pxr::HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
        pxr::HdTimeSampleArray<pxr::GfMatrix4d, 4> sampledXforms;
        sceneDelegate->SampleTransform(id, &sampledXforms);
        // if there's only one sample, it should match the cached value
        if (sampledXforms.count <= 1) {
            mGeometry->set(mGeometry->sNodeXformKey,
                           reinterpret_cast<const scene_rdl2::rdl2::Mat4d&>(sampledXforms.values[0]));
            mMirror = sampledXforms.values[0].GetDeterminant() < 0; // workaround for MOONRAY-3512
        } else {
            // first and last samples will be sample interval boundaries
            mGeometry->set(mGeometry->sNodeXformKey,
                           reinterpret_cast<const scene_rdl2::rdl2::Mat4d&>(sampledXforms.values[0]));
            mGeometry->set(mGeometry->sNodeXformKey,
                           reinterpret_cast<const scene_rdl2::rdl2::Mat4d&>(sampledXforms.values[sampledXforms.count-1]),
                           scene_rdl2::rdl2::TIMESTEP_END);
            mMirror = sampledXforms.values[0].GetDeterminant() < 0; // workaround for MOONRAY-3512
       }
    }

    for (auto&& name : mMoonrayPrimvars) {
        auto i = dirtyPrimvars.find(name);
        if (i != dirtyPrimvars.end()) {
            std::string aname(name.GetString().substr(8));
            try {
                const scene_rdl2::rdl2::Attribute* attribute(mGeometry->getSceneClass().getAttribute(aname));
                ValueConverter::setAttribute(mGeometry, attribute, i->second.value);
            } catch (const std::exception& e) {
                Logger::error(id, ": ", e.what());
            }
            i->second.value = pxr::VtValue(); // remove it from UserDatas
        }
    };
    for (auto&& name : mRemovedMoonrayPrimvars) {
        std::string aname(name.GetString().substr(8));
        try {
            const scene_rdl2::rdl2::Attribute* attribute(mGeometry->getSceneClass().getAttribute(aname));
            ValueConverter::setDefault(mGeometry, attribute);
        } catch (const std::exception& e) {
            Logger::error(id, ": ", e.what());
        }
    }
    mRemovedMoonrayPrimvars.clear();

    if (pxr::HdChangeTracker::IsDoubleSidedDirty(*dirtyBits, id)) {
        static const pxr::TfToken moonray_side_type{"moonray:side_type"};
        if (not hasPrimvar(moonray_side_type)) {
            int side_type;
            pxr::VtValue v = get(moonray_side_type, sceneDelegate);
            if (not v.IsEmpty()) {
                side_type = v.Get<int>();
            } else if (renderDelegate.isDoubleSided() || sceneDelegate->GetDoubleSided(rprim.GetId())) {
                side_type = 0; // force two-sided
            } else {
                side_type = 1; // force single-sided
            }
            mGeometry->set("side_type", side_type);
        }
    }
}


// Called after endUpdate has been done to the geometry. This creates and updates the
// UserData objects for the primvars, then sets the user data attribute on the
// geometry to the list. Any primvars stored directly in the geometry should be
// removed from this list by emptying the value in the dirtyPrimvars.
void
Geometry::updatePrimvars(DirtyPrimvars& dirtyPrimvars, RenderDelegate& renderDelegate)
{
    bool userDataSetChanged = false;

    for (auto& i : dirtyPrimvars) {
        pxr::TfToken name = i.first;
        auto& value = i.second.value;
        if (value.IsEmpty()) continue; // caller removed this one
        auto& role = i.second.role;

        auto& userData = mUserData[name];
        if (not userData) {
            std::string childName = geometry()->getName() + ".primvars:" + name.GetString();
            scene_rdl2::rdl2::SceneObject* object = renderDelegate.createSceneObject("UserData", childName);
            if (not object) continue;
            userData = object->asA<scene_rdl2::rdl2::UserData>();
            userDataSetChanged = true;
        }
        UpdateGuard guard(userData);

        // Set explicit attribute rate
        switch (i.second.interpolation) {
        case pxr::HdInterpolation::HdInterpolationConstant:
            userData->setRate(scene_rdl2::rdl2::UserData::Rate::CONSTANT);
            break;
        case pxr::HdInterpolation::HdInterpolationUniform:
            userData->setRate(scene_rdl2::rdl2::UserData::Rate::UNIFORM);
            break;
        case pxr::HdInterpolation::HdInterpolationVarying:
            userData->setRate(scene_rdl2::rdl2::UserData::Rate::VARYING);
            break;
        case pxr::HdInterpolation::HdInterpolationVertex:
            userData->setRate(scene_rdl2::rdl2::UserData::Rate::VERTEX);
            break;
        case pxr::HdInterpolation::HdInterpolationFaceVarying:
            userData->setRate(scene_rdl2::rdl2::UserData::Rate::FACE_VARYING);
            break;
        default:
            userData->setRate(scene_rdl2::rdl2::UserData::Rate::AUTO);
        }

        // there are lots of types, just get the ones encountered so far:
        if (value.IsHolding<pxr::VtFloatArray>()) {
            const pxr::VtFloatArray& v = value.UncheckedGet<pxr::VtFloatArray>();
            const float* p = &v[0];
            static const pxr::TfToken Cd("Cd");
            static const pxr::TfToken stroke_dir("stroke_dir");
            static const pxr::TfToken stroke_orig("stroke_orig");
            if (name == Cd) {
                const scene_rdl2::rdl2::Rgb* p = reinterpret_cast<const scene_rdl2::rdl2::Rgb*>(&v[0]);
                userData->setColorData(name.GetString(), scene_rdl2::rdl2::RgbVector(p, p + v.size()/3));
            } else if (name == stroke_dir || name == stroke_orig) {
                const scene_rdl2::rdl2::Vec3f* p = reinterpret_cast<const scene_rdl2::rdl2::Vec3f*>(&v[0]);
                userData->setVec3fData(name.GetString(), scene_rdl2::rdl2::Vec3fVector(p, p + v.size()/3));
            } else
            userData->setFloatData(name.GetString(), scene_rdl2::rdl2::FloatVector(p, p + v.size()));
        } else if (value.IsHolding<float>()) {
            float v = value.UncheckedGet<float>();
            userData->setFloatData(name.GetString(), scene_rdl2::rdl2::FloatVector{v});
        } else if (value.IsHolding<double>()) {
            float v = float(value.UncheckedGet<double>());
            userData->setFloatData(name.GetString(), scene_rdl2::rdl2::FloatVector{v});
        } else if (value.IsHolding<pxr::VtVec2fArray>()) {
            const pxr::VtVec2fArray& v = value.UncheckedGet<pxr::VtVec2fArray>();
            const scene_rdl2::rdl2::Vec2f* p = reinterpret_cast<const scene_rdl2::rdl2::Vec2f*>(&v[0]);
            userData->setVec2fData(name.GetString(), scene_rdl2::rdl2::Vec2fVector(p, p + v.size()));
        } else if (value.IsHolding<pxr::VtVec3fArray>()) {
            const pxr::VtVec3fArray& v = value.UncheckedGet<pxr::VtVec3fArray>();
            if (role == pxr::HdPrimvarRoleTokens->color) {
                const scene_rdl2::rdl2::Rgb* p = reinterpret_cast<const scene_rdl2::rdl2::Rgb*>(&v[0]);
                userData->setColorData(name.GetString(), scene_rdl2::rdl2::RgbVector(p, p + v.size()));
            } else {
                const scene_rdl2::rdl2::Vec3f* p = reinterpret_cast<const scene_rdl2::rdl2::Vec3f*>(&v[0]);
                userData->setVec3fData(name.GetString(), scene_rdl2::rdl2::Vec3fVector(p, p + v.size()));
            }
        } else if (value.IsHolding<pxr::GfVec3f>()) {
            const pxr::GfVec3f& v = value.UncheckedGet<pxr::GfVec3f>();
            if (role == pxr::HdPrimvarRoleTokens->color) {
                userData->setColorData(name.GetString(), scene_rdl2::rdl2::RgbVector{reinterpret_cast<const scene_rdl2::rdl2::Rgb&>(v)});
            } else {
                userData->setVec3fData(name.GetString(), scene_rdl2::rdl2::Vec3fVector{reinterpret_cast<const scene_rdl2::rdl2::Vec3f&>(v)});
            }
        } else if (value.IsHolding<pxr::VtStringArray>()) {
            const pxr::VtStringArray& v = value.UncheckedGet<pxr::VtStringArray>();
            const std::string* p = &v[0];
            userData->setStringData(name.GetString(), scene_rdl2::rdl2::StringVector(p, p + v.size()));
        } else if (value.IsHolding<std::string>()) {
            const std::string& v = value.Get<std::string>();
            userData->setStringData(name.GetString(), scene_rdl2::rdl2::StringVector{v});
        } else if (value.IsHolding<pxr::VtUIntArray>()) {
            // scene_rdl2 does not support unsigned, cast to int
            // HDM-266 moonray does not support attribute type Int for face varying attribute, cast to float
            const pxr::VtUIntArray& v = value.UncheckedGet<pxr::VtUIntArray>();
            const float* p = reinterpret_cast<const float*>(&v[0]);
            userData->setFloatData(name.GetString(), scene_rdl2::rdl2::FloatVector(p, p + v.size()));

        } else if (value.IsHolding<pxr::VtIntArray>()) {
            const pxr::VtIntArray& v = value.UncheckedGet<pxr::VtIntArray>();
            // HDM-266 moonray does not support attribute type Int for face varying attribute, cast to float
            const float* p = reinterpret_cast<const float*>(&v[0]);
            userData->setFloatData(name.GetString(), scene_rdl2::rdl2::FloatVector(p, p + v.size()));

        } else if (value.IsHolding<int>()) {
            int v = value.UncheckedGet<int>();
            userData->setIntData(name.GetString(), scene_rdl2::rdl2::IntVector{v});
        } else if (value.IsHolding<long>()) {
            int v = int(value.UncheckedGet<long>());
            userData->setIntData(name.GetString(), scene_rdl2::rdl2::IntVector{v});
        } else if (value.IsHolding<pxr::VtBoolArray>()) {
            const pxr::VtBoolArray& v = value.UncheckedGet<pxr::VtBoolArray>();
            const bool* p = &v[0];
            userData->setBoolData(name.GetString(), scene_rdl2::rdl2::BoolVector(p, p + v.size()));
        } else {
            Logger::warn(userData->getName(), ": ", value.GetTypeName(), " not translated");
        }
    }

    if (userDataSetChanged) {
        scene_rdl2::rdl2::SceneObjectVector userDatas;
        userDatas.reserve(mUserData.size());
        for (auto& i : mUserData) {
            userDatas.push_back(i.second);
        }
        UpdateGuard guard(mGeometry);
        mGeometry->set("primitive_attributes", userDatas);
    }
}

// Light sets and material and instancing is set here, for object and for all parts
//
// Subclass must first call the protected methods _UpdateInstancer and _SetMaterialId
//#if PXR_VERSION >= 2102
//    _UpdateInstancer(sceneDelegate, dirtyBits);
//#endif
//#if PXR_VERSION < 2105
//    if (*dirtyBits & pxr::HdChangeTracker::DirtyMaterialId) {
//        _SetMaterialId(sceneDelegate->GetRenderIndex().GetChangeTracker(), sceneDelegate->GetMaterialId(id));
//    }
//#endif
//
// "volume" is to switch between default surface and volume shaders if material is missing
void
Geometry::assign(pxr::HdSceneDelegate* sceneDelegate,
                 RenderDelegate& renderDelegate,
                 pxr::HdDirtyBits* dirtyBits,
                 bool volume)
{
    const pxr::SdfPath& id = rprim.GetId();
    const pxr::SdfPath& instancerId = rprim.GetInstancerId();

#if PXR_VERSION >= 2105
    if (*dirtyBits & pxr::HdChangeTracker::DirtyMaterialId) {
        rprim.SetMaterialId(sceneDelegate->GetMaterialId(id));
    }
#endif

    if (*dirtyBits & (pxr::HdChangeTracker::DirtyCategories | pxr::HdChangeTracker::DirtyMaterialId)) {
        if (mVisibleFlags) {
            scene_rdl2::rdl2::LayerAssignment assignment;

            pxr::VtArray<pxr::TfToken> categories;
            if (instancerId.IsEmpty()) {
                categories = sceneDelegate->GetCategories(id);
            } else {
                // if this geometry is instanced, we have to use GetInstanceCategories()
                std::vector<pxr::VtArray<pxr::TfToken>> instCategories =
                    sceneDelegate->GetInstanceCategories(instancerId);
                // instCategories is a vector of per-instance Category sets. Currently we can only apply a single
                // set to the entire set of instances, so we use the first
                if (instCategories.size() == 0) {
                    // point instancer
                    categories = sceneDelegate->GetCategories(instancerId);
                } else {
                    categories = instCategories[0];
                }
            }
            renderDelegate.updateAssignmentFromCategories(assignment, categories);
            Material::get(assignment, rprim.GetMaterialId(), renderDelegate, sceneDelegate, &rprim, volume);
            renderDelegate.assign(mGeometry, assignment);

            for (size_t i = 0; i < partList.size(); ++i) {
                // when instanced. light linking for parts can only be inherited from the instancer
                if (instancerId.IsEmpty()) {
                    // not instanced : there could be light linking on the GeomSubset
                    renderDelegate.updateAssignmentFromCategories(assignment, sceneDelegate->GetCategories(partPaths[i]));
                }
                Material::get(assignment, partMaterials[i], renderDelegate, sceneDelegate, &rprim, volume);
                renderDelegate.assign(geometry(), partList[i], assignment);
            }

            mAssigned = true;
        } else {
            renderDelegate.addUnassigned(mGeometry);
        }
    }

    if (pxr::HdChangeTracker::IsInstancerDirty(*dirtyBits, id) ||
        pxr::HdChangeTracker::IsInstanceIndexDirty(*dirtyBits, id) ||
        pxr::HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
        Instancer* instancer = (Instancer*)sceneDelegate->GetRenderIndex().GetInstancer(instancerId);
        if (instancer) {
            instancer->makeInstanceGeometry(id, mGeometry, this, 0);
        } // else release mInstancer?
    }
}

bool
Geometry::updateCommonPrimvars(pxr::HdSceneDelegate *sceneDelegate,
                               const pxr::SdfPath& id,
                               const pxr::TfToken& name,
                               pxr::VtValue& value,
                               scene_rdl2::rdl2::Geometry* geometry)
{
    bool found = false;
    if (name == pxr::HdTokens->points) {
        found = true;
        setMoonrayVec3fPrimvarMb(*sceneDelegate,
                                 *geometry,
                                 id,
                                 pxr::HdTokens->points,
                                 value,
                                 "vertex_list_0",
                                 "vertex_list_1");
    } else if (name == pxr::HdTokens->velocities) {
        found = true;
        setMoonrayVec3fPrimvarMb(*sceneDelegate,
                                 *geometry,
                                 id,
                                 pxr::HdTokens->velocities,
                                 value,
                                 "velocity_list_0",
                                 "velocity_list_1");
    } else if (name == pxr::HdTokens->accelerations) {
        found = true;
        setMoonrayVec3fPrimvarMb(*sceneDelegate,
                                 *geometry,
                                 id,
                                 pxr::HdTokens->accelerations,
                                 value,
                                 "accleration_list",
                                 "accleration_list"); // only 1 sample for acceleration
    }

    if (found) {
        value = pxr::VtValue(); // remove value from UserData primvars
        return true;
    } else {
        return false;
    }
}

}
