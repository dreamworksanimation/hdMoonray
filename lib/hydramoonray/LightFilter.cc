// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "LightFilter.h"
#include "RenderDelegate.h"
#include "ValueConverter.h"
#include "Material.h"
#include "Camera.h"
#include "HdmLog.h"

#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/usd/usdLux/tokens.h>

// nb, must use full name scene_rdl2::rdl2::LightFilter...
#include <scene_rdl2/scene/rdl2/LightFilter.h>
#include <scene_rdl2/scene/rdl2/Layer.h>
#include <scene_rdl2/scene/rdl2/Material.h>
#include <scene_rdl2/render/logging/logging.h>
#include <iostream>

using namespace scene_rdl2::rdl2;

namespace {
using scene_rdl2::logging::Logger;

pxr::TfToken moonrayClassToken("moonray:class");
pxr::TfToken fallbackClass("DecayLightFilter");

#if PXR_VERSION >= 2005
# define lightFilterToken (pxr::HdPrimTypeTokens->lightFilter)
# define lightFilterLinkToken (pxr::HdTokens->lightFilterLink)
#else
pxr::TfToken lightFilterToken("lightFilter");
pxr::TfToken lightFilterLinkToken("lightFilterLink");
#endif
}

namespace hdMoonray {

using scene_rdl2::logging::Logger;

pxr::HdDirtyBits
LightFilter::GetInitialDirtyBitsMask() const
{
    return pxr::HdChangeTracker::DirtyParams;
}

void
LightFilter::syncParams(const pxr::SdfPath& id,
                  pxr::HdSceneDelegate *sceneDelegate,
                  RenderDelegate& renderDelegate)
{
    const SceneClass& sceneClass = mLightFilter->getSceneClass();

    for (auto it = sceneClass.beginAttributes();
         it != sceneClass.endAttributes(); ++it) {

        const std::string& attrName = (*it)->getName();
        if (attrName == "node_xform") {
            syncXform(id, sceneDelegate);
        } else if (attrName == "projector") {
            syncProjector(id,sceneDelegate, renderDelegate);
        } else if (attrName == "texture_map") {
            syncTextureMap(id, sceneDelegate, renderDelegate);
        } else  if (attrName == "light_filters") {
            syncCombineFilters(id, sceneDelegate, renderDelegate);
        } else {
            pxr::TfToken moonrayName("moonray:" + attrName);
            pxr::VtValue val = sceneDelegate->GetLightParamValue(id, moonrayName);
            if (val.IsEmpty()) {
                ValueConverter::setDefault(mLightFilter, *it);
            } else {
                ValueConverter::setAttribute(mLightFilter, *it, val);
            }
        }
    }
}

void
LightFilter::syncProjector(const pxr::SdfPath& id,
                         pxr::HdSceneDelegate *sceneDelegate,
                         RenderDelegate& renderDelegate) 
{
    // sync the "projector" attribute of Cookie light filters,
    // which is authored as a "rel" to a camera. Note that this
    // requires the custom MoonrayLightFilterAdapter to work
    static pxr::TfToken projectorToken("moonray:projector");
    pxr::VtValue val = sceneDelegate->Get(id, projectorToken); // supplied by adapter
    if (val.IsHolding<pxr::SdfPath>()) {
        pxr::SdfPath path = val.UncheckedGet<pxr::SdfPath>();
        path.ReplacePrefix(pxr::SdfPath::AbsoluteRootPath(), sceneDelegate->GetDelegateID());
        SceneObject* so = hdMoonray::Camera::createCamera(sceneDelegate, renderDelegate, path);
        mLightFilter->set("projector", so);
        if (not so) {
            Logger::error(GetId(), ".moonray:projector: ", path, " not found");
        }
    } else if (not val.IsEmpty()) {
        Logger::error(GetId(), ".moonray:projector: must be a path");
    }
}

void
LightFilter::syncXform(const pxr::SdfPath& id,
                       pxr::HdSceneDelegate *sceneDelegate)
{
    // some light filters (e.g. Rod) have a blurrable node_xform attr
    // (although not actually inherited from Node, so don't use Node's attribute key)
    // Do not call this function without first verifying that the
    // light filter has an attribute called "node_xform"
    pxr::HdTimeSampleArray<pxr::GfMatrix4d, 4> sampledXforms;
    sceneDelegate->SampleTransform(id, &sampledXforms);
    // if there's only one sample, it should match the cached value
    if (sampledXforms.count <= 1) {
        scene_rdl2::rdl2::Mat4d rdl2Xform0 =
            reinterpret_cast<scene_rdl2::rdl2::Mat4d&>(sampledXforms.values[0]);
        mLightFilter->set("node_xform", rdl2Xform0);
    } else {
        // first and last samples will be sample interval boundaries
        scene_rdl2::rdl2::Mat4d rdl2Xform0 =
            reinterpret_cast<scene_rdl2::rdl2::Mat4d&>(sampledXforms.values[0]);
        mLightFilter->set("node_xform", rdl2Xform0);
        scene_rdl2::rdl2::Mat4d rdl2Xform1 =
            reinterpret_cast<scene_rdl2::rdl2::Mat4d&>(sampledXforms.values[sampledXforms.count-1]);
        mLightFilter->set("node_xform", rdl2Xform1, scene_rdl2::rdl2::TIMESTEP_END);
   }
}

void
LightFilter::syncCombineFilters(const pxr::SdfPath& id,
                                pxr::HdSceneDelegate *sceneDelegate,
                                RenderDelegate& renderDelegate)
{
    // sync the "light_filters" attribute of Combine light filters,
    // which is authored as "rel"s to light filters. Note that this
    // requires the custom MoonrayLightFilterAdapter to work
    static pxr::TfToken filtersToken("moonray:light_filters");
    pxr::VtValue val = sceneDelegate->Get(id, filtersToken); // supplied by adapter
    if (val.IsHolding<pxr::SdfPathVector>()) {
        scene_rdl2::rdl2::SceneObjectVector rdlObjects;
        pxr::SdfPathVector pathVec = val.UncheckedGet<pxr::SdfPathVector>();
        for (const pxr::SdfPath& cpath : pathVec) {
            pxr::SdfPath path(cpath);
            path.ReplacePrefix(pxr::SdfPath::AbsoluteRootPath(), sceneDelegate->GetDelegateID());
            SceneObject* so = LightFilter::getFilter(sceneDelegate, renderDelegate, path);
            if (so) {
                rdlObjects.push_back(so);
            } else {
                Logger::error(GetId(), ".moonray:light_filters: ", path, " not found");
            }
        }
        mLightFilter->set("light_filters", rdlObjects);
    } else if (not val.IsEmpty()) {
        Logger::error(GetId(), ".moonray:light_filters: must be a list of paths");
    }
}

void
LightFilter::syncTextureMap(const pxr::SdfPath& id,
                            pxr::HdSceneDelegate *sceneDelegate,
                            RenderDelegate& renderDelegate)
{

    const std::string moonrayName = "moonray:texture_map";
    scene_rdl2::rdl2::SceneObject* shader = nullptr;
    pxr::VtValue hdMatVal = sceneDelegate->GetMaterialResource(id);
    if (!hdMatVal.IsHolding<pxr::HdMaterialNetworkMap>()) {
        return;
    }
    const pxr::HdMaterialNetworkMap& networkmap = hdMatVal.UncheckedGet<pxr::HdMaterialNetworkMap>();
    pxr::SdfPath inputId;
    for (auto const& iter : networkmap.map) {
        const pxr::HdMaterialNetwork & network = iter.second;
        for (const pxr::HdMaterialRelationship& rel : network.relationships) {
            if (rel.outputName == moonrayName){
                inputId = rel.inputId;
                break;
            }
        }
        if (inputId.IsEmpty())
            continue;

        // found the connected network, import all the shader nodes
        // and set the connected one
        for (const pxr::HdMaterialNode & node : network.nodes) {
            // we have to skip the actual light filter node
            if (node.identifier == "MoonrayLightFilter" || 
                node.path == id) {
                continue;
            }
            const std::string nodeName = node.path.GetString();
            shader = makeMoonrayShader(renderDelegate, sceneDelegate, node, nodeName, nullptr);
            if (!shader) {
                continue;
            }
            if (node.path == inputId){
                mLightFilter->set("texture_map", shader);
            }
        }
    }

}

// Hydra doesn't currently seem to analyse the dependency between light and light filter
// correctly, so a light may be synced before the filters it references. To work around
// this, we need to allow the light to create the filter outside Sync, under a mutex...
scene_rdl2::rdl2::LightFilter*
LightFilter::getOrCreateFilter(pxr::HdSceneDelegate *sceneDelegate,
                               RenderDelegate& renderDelegate,
                               const pxr::SdfPath& id)
{
    std::lock_guard<std::mutex> lock(mCreateMutex);
    if (not mLightFilter) {
        pxr::VtValue vtClass = sceneDelegate->GetLightParamValue(id, moonrayClassToken);
        pxr::TfToken classToken;
        if (vtClass.IsHolding<pxr::TfToken>()) {
            classToken = vtClass.UncheckedGet<pxr::TfToken>();
        } else {
            classToken = fallbackClass;
            Logger::warn("hdMoonray: Unspecified LightFilter type : creating ", classToken);
        }
        mLightFilter = renderDelegate.createSceneObject(classToken.GetString(), id)->asA<scene_rdl2::rdl2::LightFilter>();

        // See Light.cc for explanation...
        pxr::VtValue val = sceneDelegate->GetLightParamValue(id, lightFilterLinkToken);
        if (val.IsHolding<pxr::TfToken>()) {
            mLightFilterCategory = val.UncheckedGet<pxr::TfToken>();
        }
        renderDelegate.setCategory(mLightFilter,RenderDelegate::CategoryType::FilterLink,mLightFilterCategory);
    }
    return mLightFilter;
}

void
LightFilter::Sync(pxr::HdSceneDelegate *sceneDelegate,
                  pxr::HdRenderParam   *renderParam,
                  pxr::HdDirtyBits     *dirtyBits)
{
    pxr::SdfPath id = GetId();
    hdmLogSyncStart("LightFilter", id, dirtyBits);
    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));

    getOrCreateFilter(sceneDelegate,renderDelegate,id);

    if ((*dirtyBits) & pxr::HdChangeTracker::DirtyParams) {
        UpdateGuard guard(renderDelegate, mLightFilter);
        syncParams(id,sceneDelegate, renderDelegate);
    }

    *dirtyBits = pxr::HdChangeTracker::Clean;
    hdmLogSyncEnd(id);
}

void
LightFilter::Finalize(pxr::HdRenderParam *renderParam)
{
    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));
    renderDelegate.releaseCategory(mLightFilter,RenderDelegate::CategoryType::FilterLink,mLightFilterCategory);
}

/*static*/ LightFilter*
get(pxr::HdSceneDelegate* sceneDelegate, const pxr::SdfPath& filterId)
{
    if (not filterId.IsEmpty()) {
        LightFilter* filterPrim = static_cast<LightFilter*>(
            sceneDelegate->GetRenderIndex().GetSprim(lightFilterToken, filterId));
        if (filterPrim) {
            return filterPrim;
        }
        Logger::error(filterId, ": no such LightFilter");
    }
    return nullptr;
}

scene_rdl2::rdl2::LightFilter*
LightFilter::getFilter(pxr::HdSceneDelegate* sceneDelegate,
                       RenderDelegate& renderDelegate,
                       const pxr::SdfPath& filterId)
{
    LightFilter* filterPrim = get(sceneDelegate, filterId);
    if (filterPrim)
        return filterPrim->getOrCreateFilter(sceneDelegate,renderDelegate,filterId);
    return nullptr;
}

}

