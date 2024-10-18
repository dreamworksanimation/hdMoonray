// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Volume.h"
#include "RenderDelegate.h"
#include "HdmLog.h"
#include "pxr/usd/sdf/types.h" // for SdfAssetPath
#include <scene_rdl2/scene/rdl2/Geometry.h>
#include <iostream>

using namespace pxr;

namespace {
    
const std::string rdlClassVdb("VdbGeometry");
const std::string rdlAttrModel("model");

}

namespace hdMoonray {

using scene_rdl2::logging::Logger;

Volume::Volume(SdfPath const& id) :
    HdVolume(id), 
    GeometryMixin(this) 
{}

void
Volume::Finalize(HdRenderParam* renderParam)
{
    // causes geometry to be hidden
    resetGeometryObject(RenderDelegate::get(renderParam));
}

HdDirtyBits
Volume::GetInitialDirtyBitsMask() const
{
    return HdChangeTracker::AllDirty;
}

void 
Volume::syncAttributes(HdSceneDelegate* sceneDelegate, 
                            RenderDelegate& renderDelegate,
                            HdDirtyBits* dirtyBits,
                            const TfToken& reprToken)
{
    static const TfToken densityToken("density");
    static const TfToken velocityToken("velocity");
    
    auto fields(sceneDelegate->GetVolumeFieldDescriptors(GetId()));
    const std::string* filePath = nullptr;
    for (auto& desc: fields) {
        const OpenVdbAsset* const field =
            dynamic_cast<OpenVdbAsset*>(sceneDelegate->GetRenderIndex().GetBprim(
                                            desc.fieldPrimType, desc.fieldId));
        if (!field) continue;
           
        if (desc.fieldName == densityToken || 
            desc.fieldName == velocityToken) {
                
            const std::string attributeName = desc.fieldName.GetString() + "_grid";
            std::string grid(field->name().GetString());
            if (field->index() > 0) {
                grid += "[" + std::to_string(field->index()) + "]";
            }
            geometry()->set(attributeName, grid);
                    
            const std::string& fp = field->filePath();
            if (not fp.empty()) {
                if (filePath && *filePath != fp) {
                    Logger::error(GetId(), " all fields must use same vdb file");
                    break;
                }
                filePath = &fp;
            }
            if (filePath) {
                geometry()->set(rdlAttrModel, *filePath);
            }
        }
    }

    // base class handles transform and double-sided
    GeometryMixin::syncAttributes(sceneDelegate, renderDelegate, dirtyBits, reprToken);
   
}

void
Volume::Sync(HdSceneDelegate* sceneDelegate,
             HdRenderParam*   renderParam,
             HdDirtyBits*     dirtyBits,
             const TfToken&   reprToken)
{
    hdmLogSyncStart("Volume", GetId(), dirtyBits);   
    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));
    
    _UpdateVisibility(sceneDelegate, dirtyBits);
    _UpdateInstancer(sceneDelegate, dirtyBits);
    
    syncAll(rdlClassVdb, sceneDelegate, renderDelegate, dirtyBits, reprToken);
    
    hdmLogSyncEnd(GetId());
}

void
OpenVdbAsset::Sync(HdSceneDelegate *sceneDelegate,
                   HdRenderParam   *renderParam,
                   HdDirtyBits     *dirtyBits)
{
    const TfToken fieldNameToken("fieldName");
    const TfToken fieldIndexToken("fieldIndex");

    const SdfPath& id = GetId();
    hdmLogSyncStart("OpenVdbAsset", id, dirtyBits);

    if (*dirtyBits & DirtyParams) {
        VtValue v;
        v = sceneDelegate->Get(id, HdFieldTokens->filePath);
        mFilePath = v.Get<SdfAssetPath>().GetResolvedPath();
        v = sceneDelegate->Get(id, fieldNameToken);
        mName = v.Get<TfToken>();
        v = sceneDelegate->GetLightParamValue(id, fieldIndexToken);
        mIndex = (not v.IsEmpty()) ? v.Get<int>() : -1;
    }
    *dirtyBits = Clean;
    hdmLogSyncEnd(id);
}

}
