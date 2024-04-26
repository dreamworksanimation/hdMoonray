// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "LightFilter.h"
#include "RenderDelegate.h"
#include "ValueConverter.h"
#include "Material.h"
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
        std::string moonrayName = "moonray:"+attrName;
        if (attrName == "texture_map") {
            syncTexture(attrName, id, sceneDelegate, renderDelegate);
        } else {
            pxr::VtValue val = sceneDelegate->GetLightParamValue(id, pxr::TfToken(moonrayName));
            if (val.IsEmpty()) {
                ValueConverter::setDefault(mLightFilter, *it);
            } else {
                ValueConverter::setAttribute(mLightFilter, *it, val);
            }
        }
    }
}

void
LightFilter::syncTexture(const std::string& attrName,
                         const pxr::SdfPath& id,
                         pxr::HdSceneDelegate *sceneDelegate,
                         RenderDelegate& renderDelegate){

    std::string moonrayName = "moonray:"+attrName;
    scene_rdl2::rdl2::SceneObject* shader = nullptr;
    pxr::VtValue hdMatVal = sceneDelegate->GetMaterialResource(id);
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
            if (node.identifier == "MoonrayLightFilter")
                continue;
            const std::string nodeName = node.path.GetString();
            shader = makeMoonrayShader(renderDelegate, sceneDelegate, node, nodeName, nullptr);
            if (!shader) {
                continue;
            }
            if (node.path == inputId){
                mLightFilter->set(attrName, shader);
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

