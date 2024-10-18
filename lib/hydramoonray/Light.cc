// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Light.h"
#include "LightFilter.h"
#include "Mesh.h"
#include "RenderDelegate.h"
#include "ValueConverter.h"
#include "HdmLog.h"

#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/base/gf/matrix4d.h>

#include <pxr/usd/usdLux/blackbody.h>

// nb, must use full name scene_rdl2::rdl2::Light...
#include <scene_rdl2/scene/rdl2/Light.h>
#include <scene_rdl2/scene/rdl2/LightFilter.h>
#include <scene_rdl2/scene/rdl2/Geometry.h>
#include <scene_rdl2/common/math/Math.h>

#include <scene_rdl2/render/logging/logging.h>
#include <iostream>

using namespace scene_rdl2::rdl2;

namespace {

pxr::TfToken moonrayClassToken("moonray:class");
pxr::TfToken spotLightToken("SpotLight");
pxr::TfToken rectLightToken("RectLight");
pxr::TfToken geometryLightToken("geometryLight");
pxr::TfToken geometryToken("inputs:geometry");
pxr::TfToken moonrayGeometryToken("moonray:geometry");

const std::string&
defaultRdlClassName(const pxr::TfToken& type)
{
    static const std::string def;
    static std::map<pxr::TfToken,std::string> rdlClassMap =
        { { pxr::HdPrimTypeTokens->cylinderLight , "CylinderLight" },
          { pxr::HdPrimTypeTokens->diskLight, "DiskLight" },
          { pxr::HdPrimTypeTokens->distantLight, "DistantLight" },
          { pxr::HdPrimTypeTokens->domeLight, "EnvLight" },
          { pxr::HdPrimTypeTokens->rectLight, "RectLight" },
          { pxr::HdPrimTypeTokens->sphereLight, "SphereLight" },
          { geometryLightToken, "MeshLight" }};
    auto it = rdlClassMap.find(type);
    if (it == rdlClassMap.end()) return def;
    return it->second;
}

}

namespace hdMoonray {

using scene_rdl2::logging::Logger;

pxr::HdDirtyBits
Light::GetInitialDirtyBitsMask() const
{
    return pxr::HdLight::AllDirty;
}


const std::string&
Light::rdlClassName(const pxr::SdfPath& id,
                    pxr::HdSceneDelegate *sceneDelegate)
{
    // identify the rdl light class to use. This can be specified via "token moonray::class =" or
    // deduced from the Lux/Usd type
    const std::string& luxRdlClass(defaultRdlClassName(mType));
    pxr::VtValue v = sceneDelegate->GetLightParamValue(id, moonrayClassToken);
    bool isRectLight = false;
    if (v.IsHolding<pxr::TfToken>()) {
        pxr::TfToken classToken = v.UncheckedGet<pxr::TfToken>();
        if (classToken == rectLightToken) {
            isRectLight = true;
        }
        // See if the moonray::class does not match the Lux type:
        bool error;
        if (classToken == spotLightToken) {
            error = (mType != pxr::HdPrimTypeTokens->diskLight && mType != pxr::HdPrimTypeTokens->sphereLight);
        } else {
            error = (classToken != luxRdlClass);
        }
        if (error) {
            Logger::warn(id, ".moonray:class: '", classToken,
                         "' may not be compatible with USD light type '", mType, "'");
        }
        return classToken.GetString();
    }
    // existence of shaping api makes a SpotLight
    v = sceneDelegate->GetLightParamValue(id, pxr::HdLightTokens->shapingConeAngle);
    if (v.IsHolding<float>() && v.UncheckedGet<float>() < 90.0f) {
        if (mType != pxr::HdPrimTypeTokens->diskLight) {
            Logger::warn(id, ": shaping api may not be compatible with USD light type '", mType, "'");
        }
        if (isRectLight) {
            mRectToSpotlight = true;
        }
        return spotLightToken.GetString();
    }
    if (luxRdlClass.empty()) {
        Logger::error(id, ": Unsupported light type ", mType, " replaced by DiskLight");
        return defaultRdlClassName(pxr::HdPrimTypeTokens->diskLight);
    }
    return luxRdlClass;
}

bool
Light::isSupportedType(const pxr::TfToken& type)
{
    return !defaultRdlClassName(type).empty();
}


void
Light::fixCylinderLight(scene_rdl2::rdl2::Mat4d& mat) {
    // in Usd/Lux cylinder is along x-axis, in moonray it is along y.
    if (mType == pxr::HdPrimTypeTokens->cylinderLight) {
        // rotate -90deg about z
        std::swap(mat.vx, mat.vy);
        mat.vx *= -1.0;
    }
}

void
Light::syncXform(const pxr::SdfPath& id,
                 pxr::HdSceneDelegate *sceneDelegate)
{
    pxr::HdTimeSampleArray<pxr::GfMatrix4d, 4> sampledXforms;
    sceneDelegate->SampleTransform(id, &sampledXforms);
    // if there's only one sample, it should match the cached value
    if (sampledXforms.count <= 1) {
        scene_rdl2::rdl2::Mat4d rdl2Xform0 =
            reinterpret_cast<scene_rdl2::rdl2::Mat4d&>(sampledXforms.values[0]);
        fixCylinderLight(rdl2Xform0);
        mLight->set(mLight->sNodeXformKey, rdl2Xform0);
    } else {
        // first and last samples will be sample interval boundaries
        scene_rdl2::rdl2::Mat4d rdl2Xform0 =
            reinterpret_cast<scene_rdl2::rdl2::Mat4d&>(sampledXforms.values[0]);
        fixCylinderLight(rdl2Xform0);
        mLight->set(mLight->sNodeXformKey, rdl2Xform0);
        scene_rdl2::rdl2::Mat4d rdl2Xform1 =
            reinterpret_cast<scene_rdl2::rdl2::Mat4d&>(sampledXforms.values[sampledXforms.count-1]);
        fixCylinderLight(rdl2Xform1);
        mLight->set(mLight->sNodeXformKey, rdl2Xform1, scene_rdl2::rdl2::TIMESTEP_END);
   }
}

// If all the lights are off the renderDelegate has to turn the default dome light on
void
Light::setOn(bool value, RenderDelegate& renderDelegate) {
    if (value != mOn) {
        mOn = value;
        if (value)
            renderDelegate.addLight();
        else
            renderDelegate.removeLight();
        mLight->set(mLight->sOnKey, value);
    }
}

pxr::GfVec3f
colorTemperatureToRGB(float kelvin)
{
    float t = scene_rdl2::math::clamp(kelvin, 1000.0f, 40000.0f) / 100.0f;
    float r, g, b;

    if (t <= 66.0f) {
        r = 1.0f;
        g = 0.39008157876901960784f * log(t) - 0.6318414437886274509f;
    } else {
        r = 1.29293618606274509804f * pow(t - 60.0f, -0.1332047592f);
        g = 1.12989086089529411765f * pow(t - 60.0f, -0.0755148492f);
    }

    if (t >= 66.0f)
        b = 1.0f;
    else if(t <= 19.0f)
        b = 0.0f;
    else
        b = 0.54320678911019607843f * log(t - 10.0f) - 1.19625408914f;

    return pxr::GfVec3f(scene_rdl2::math::clamp(r, 0.0f, 1.0f),
                        scene_rdl2::math::clamp(g, 0.0f, 1.0f),
                        scene_rdl2::math::clamp(b, 0.0f, 1.0f));
}

void
Light::syncParams(const pxr::SdfPath& id,
                  pxr::HdSceneDelegate *sceneDelegate,
                  RenderDelegate& renderDelegate)
{
    const SceneClass& sceneClass = mLight->getSceneClass();

    for (auto it = sceneClass.beginAttributes(); it != sceneClass.endAttributes(); ++it) {
        const std::string& attrName = (*it)->getName();

        // attributes directly updated by Sync():
        if (attrName == "on" || attrName == "node_xform" || attrName == "intensity") continue;

        if (attrName == "geometry") {
            // geometry light support is not reliable in this version of Hydra, since access to
            // a geometry prim from a light is not anticipated. Correct solution is to use a
            // Mesh prim with LightAPI applied
            pxr::VtValue relval = sceneDelegate->Get(id, geometryToken);
            // This relies on our custom GeometryLight adapter, which returns a path taken from
            // the rel geometry property. It can be broken by the pxr adapter taking precedence,
            // which will simply report that GeometryLight is not supported yet.
            if (relval.IsHolding<pxr::SdfPath>()) {
                // Given the path from rel geometry, we need to get the corresponding rprim, and
                // thence the RDL geometry object. This implementation is not safe, because it
                // assumes that the geom uses the same delegate as the light, but I don't think 
                // there is an alternative in Hydra 1
                pxr::SdfPath geomPath = relval.UncheckedGet<pxr::SdfPath>();
                geomPath.ReplacePrefix(pxr::SdfPath::AbsoluteRootPath(), sceneDelegate->GetDelegateID());
                pxr::HdRprim* prim = const_cast<pxr::HdRprim*>(sceneDelegate->GetRenderIndex().GetRprim(geomPath));
                Mesh* mesh = dynamic_cast<Mesh*>(prim);
                if (mesh) {
                    // geometry sync should not be running in parallel with light sync
                    scene_rdl2::rdl2::SceneObject* geom = mesh->geometryForMeshLight(renderDelegate);
                    if (geom) {
                        mLight->set("geometry",geom);
                    } 
                } 
            } 
            continue;
        }

        // check if attr is set by a moonray::name property
        std::string moonrayName = "moonray:"+attrName;
        pxr::VtValue val = sceneDelegate->GetLightParamValue(id, pxr::TfToken(moonrayName));
        if (!val.IsEmpty()) {
            ValueConverter::setAttribute(mLight, *it, val);
            continue;
        }

        // Find the equivalent Lux attribute
        static const std::map<std::string,pxr::TfToken> map = {
            { "color", pxr::HdLightTokens->color },
            //{ "intensity", pxr::HdLightTokens->intensity }, // handled in sync
            { "exposure", pxr::HdLightTokens->exposure },
            { "radius", pxr::HdLightTokens->radius },
            { "normalized", pxr::HdLightTokens->normalize },
            { "width", pxr::HdLightTokens->width },
            { "height", pxr::HdLightTokens->height },
            { "angular_extent", pxr::HdLightTokens->angle },
            { "texture", pxr::HdLightTokens->textureFile },
            { "lens_radius", pxr::HdLightTokens->radius },
            { "outer_cone_angle", pxr::HdLightTokens->shapingConeAngle },
            { "inner_cone_angle", pxr::HdLightTokens->shapingConeSoftness }
        };
        auto it2 = map.find(attrName);
        if (it2 != map.end()) {
            pxr::TfToken luxName = it2->second;

            if (luxName == pxr::HdLightTokens->shapingConeAngle) {
                float coneAngle = 90; // Lux default value
                val = sceneDelegate->GetLightParamValue(id, pxr::HdLightTokens->shapingConeAngle);
                if (val.IsHolding<float>()) coneAngle = val.UncheckedGet<float>();
                mLight->set(AttributeKey<float>(**it), 2 * coneAngle);
                continue;

            } else if (luxName == pxr::HdLightTokens->shapingConeSoftness) {
                float softness = 0; // Lux default value
                val = sceneDelegate->GetLightParamValue(id, luxName);
                if (val.IsHolding<float>()) softness = val.UncheckedGet<float>();
                float coneAngle = 90; // Lux default value
                val = sceneDelegate->GetLightParamValue(id, pxr::HdLightTokens->shapingConeAngle);
                if (val.IsHolding<float>()) coneAngle = val.UncheckedGet<float>();
                float innerConeAngle = coneAngle;
                if (softness > 0) {
                    innerConeAngle = (softness < 1) ? coneAngle * (1 - softness) : 0.0f;
                }
                mLight->set(AttributeKey<float>(**it), 2 * innerConeAngle);
                continue;

            } else if (luxName == pxr::HdLightTokens->radius && mRectToSpotlight == true) {
                // Since rect lights with shaping are converted to spotlights, we approximate
                // the rect light area using the spotlight's lens_radius parameter.
                float width = 1.0f;
                val = sceneDelegate->GetLightParamValue(id, pxr::HdLightTokens->width);
                if (val.IsHolding<float>()) width = val.UncheckedGet<float>();
                float height = 1.0f;
                val = sceneDelegate->GetLightParamValue(id, pxr::HdLightTokens->height);
                if (val.IsHolding<float>()) height = val.UncheckedGet<float>();
                const float radius = scene_rdl2::math::sqrt((width * height) / scene_rdl2::math::sPi);
                mLight->set(AttributeKey<float>(**it), radius);
                continue;
            } else if (luxName == pxr::HdLightTokens->color) {
                pxr::GfVec3f color(1.0f);
                val = sceneDelegate->GetLightParamValue(id, luxName);
                if (val.IsHolding<pxr::GfVec3f>()) color = val.UncheckedGet<pxr::GfVec3f>();
                val = sceneDelegate->GetLightParamValue(id, pxr::HdLightTokens->enableColorTemperature);
                if (val.IsHolding<bool>() && val.UncheckedGet<bool>()) {
                    val = sceneDelegate->GetLightParamValue(id, pxr::HdLightTokens->colorTemperature);
                    if (val.IsHolding<float>()) {
                        const float temperature = val.UncheckedGet<float>();

                        // This was originally using the function pxr::UsdLuxBlackbodyTemperatureAsRgb
                        // to do the conversion.   However there was a noticeable difference in the color
                        // compared to the Karma renderer.   To better match this we use the algorithm
                        // from the following site:
                        // https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html
                        pxr::GfVec3f  tempRgb = colorTemperatureToRGB(temperature);
                        color[0] *= tempRgb[0];
                        color[1] *= tempRgb[1];
                        color[2] *= tempRgb[2];
                    }
                }
                mLight->set(AttributeKey<scene_rdl2::rdl2::Rgb>(**it), reinterpret_cast<scene_rdl2::rdl2::Rgb&>(color));
                continue;

            } else {
                // special case: Lux CylinderLight uses "length" where rdl2 uses "height"
                if (luxName == pxr::HdLightTokens->height && mType == pxr::HdPrimTypeTokens->cylinderLight) {
                    luxName = pxr::HdLightTokens->length;
                }
                // schema will cause correct default to be returned
                val = sceneDelegate->GetLightParamValue(id, luxName);
                if (!val.IsEmpty()) {
                    ValueConverter::setAttribute(mLight, *it, val);
                    continue;
                }
            }
        }

        // no setting, so reset to default
        ValueConverter::setDefault(mLight, *it);
    }
}

void
Light::syncFilterList(const pxr::SdfPath& id,
                      pxr::HdSceneDelegate *sceneDelegate,
                      RenderDelegate& renderDelegate)
{
    pxr::VtValue val = sceneDelegate->GetLightParamValue(id, pxr::TfToken(pxr::HdTokens->filters));
    if (!val.IsHolding<pxr::SdfPathVector>()) {
        return;
    }
    scene_rdl2::rdl2::SceneObjectVector filters;
    const pxr::SdfPathVector& paths = val.UncheckedGet<pxr::SdfPathVector>();
    for (const pxr::SdfPath& path : paths) {
        scene_rdl2::rdl2::LightFilter* filter = LightFilter::getFilter(sceneDelegate,renderDelegate,path);
        if (filter) {
            filters.push_back(filter);
        }
    }
    mLight->set(scene_rdl2::rdl2::Light::sLightFiltersKey,filters);
}

void
Light::Sync(pxr::HdSceneDelegate *sceneDelegate,
            pxr::HdRenderParam   *renderParam,
            pxr::HdDirtyBits     *dirtyBits)
{
    pxr::SdfPath id = GetId();
    hdmLogSyncStart("Light", id, dirtyBits);

    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));

    // HDM-125: usdview sets the intensity of lights to 0.0f if "Enable Scene Lights" is turned off,
    // so treat intensity=0 as turning off the light. Also turn it off when lighting disabled.
    float intensity = 0.0f;
    if (not renderDelegate.getDisableLighting() && sceneDelegate->GetVisible(id)) {
        pxr::VtValue val = sceneDelegate->GetLightParamValue(id, pxr::HdLightTokens->intensity);
        intensity = val.IsHolding<float>() ? val.UncheckedGet<float>() : 1.0f;
    }

    bool initialize = false;
    if (not mLight) {
        *dirtyBits = DirtyBits::Clean; // don't call Sync again if no light is created
        if (not (intensity > 0)) return; // don't create invisible lights
        // currently if the class changes, Finalize() is called, so there is no need to check
        // for this after the object is created.
        const std::string& rdlClass = rdlClassName(id, sceneDelegate);
        scene_rdl2::rdl2::SceneObject* object = renderDelegate.createSceneObject(rdlClass, id);
        mLight = object ? object->asA<scene_rdl2::rdl2::Light>() : nullptr;
        if (not mLight) return; // if there was an error this already printed an error message
        initialize = true; // Force the catagories to be updated
        *dirtyBits = pxr::HdLight::AllDirty; // force other settings to be made
    }

    UpdateGuard guard(renderDelegate, mLight);

    if ((*dirtyBits) & pxr::HdLight::DirtyTransform) {
        syncXform(id, sceneDelegate);
    }

    if ((*dirtyBits) & pxr::HdLight::DirtyParams) {
        setOn(intensity > 0, renderDelegate);
        mLight->set(scene_rdl2::rdl2::Light::sIntensityKey, intensity);
        syncParams(id, sceneDelegate, renderDelegate);
        syncFilterList(id, sceneDelegate, renderDelegate);
        // querying "lightLink" will return a token used to name the "category" that
        // holds all geometry that this light links to. This value will later be
        // returned as one of the entries if any of the linked geometry calls GetCategories().
        // In other words, the scene delegate does the reverse lookup "geometry->lights" for
        // us using an internal cache.
        // Value is either "" or "<id>.collection:lightLink" though this will allow others.
        // Currently Finalize() is called when value changes, but this may be a bug.
        bool categoriesChanged = false;
        pxr::TfToken t =
            sceneDelegate->GetLightParamValue(id, pxr::HdTokens->lightLink).Get<pxr::TfToken>();
        // registering the category id token with RenderDelegate will enable geometry
        // to look up mLight as the rdl2 scene object corresponding to this category id
        if (initialize || t != mLightLinkCategory) {
            if (!initialize) {
                renderDelegate.releaseCategory(mLight, RenderDelegate::CategoryType::LightLink, mLightLinkCategory);
            }
            renderDelegate.setCategory(mLight, RenderDelegate::CategoryType::LightLink, t);
            mLightLinkCategory = t;
            categoriesChanged = true;
        }
        // "shadowLink" is much the same
        t = sceneDelegate->GetLightParamValue(id, pxr::HdTokens->shadowLink).Get<pxr::TfToken>();
        if (initialize || t != mShadowLinkCategory) {
            if (!initialize) {
                renderDelegate.releaseCategory(mLight, RenderDelegate::CategoryType::ShadowLink, mShadowLinkCategory);
            }
            renderDelegate.setCategory(mLight, RenderDelegate::CategoryType::ShadowLink, t);
            mShadowLinkCategory = t;
            categoriesChanged = true;
        }
        // Need to call Sync() on all geometry to get categories copied into LightSets
        if (categoriesChanged) {
            // in 0.22.5, DirtyCategories seems to be ignored. We can force a sync using
            // DirtyMaterialId even though it isn't strict;y right...
            sceneDelegate->GetRenderIndex().GetChangeTracker().MarkAllRprimsDirty(
                pxr::HdChangeTracker::DirtyCategories | pxr::HdChangeTracker::DirtyMaterialId);
        }
    }

    *dirtyBits = DirtyBits::Clean;
    hdmLogSyncEnd(id);
}

void
Light::Finalize(pxr::HdRenderParam *renderParam)
{
    if (mLight) {
        RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));
        UpdateGuard guard(renderDelegate, mLight);
        setOn(false, renderDelegate);
        renderDelegate.releaseCategory(mLight, RenderDelegate::CategoryType::LightLink, mLightLinkCategory);
        renderDelegate.releaseCategory(mLight, RenderDelegate::CategoryType::ShadowLink, mShadowLinkCategory);
        mLight = nullptr;
    }
}

}
