// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Volume.h"
#include "RenderDelegate.h"
#include "HdmLog.h"

#include "pxr/usd/sdf/types.h" // for SdfAssetPath

#include <scene_rdl2/scene/rdl2/Geometry.h>

#include <iostream>

namespace {
static const pxr::TfToken fieldNameToken("fieldName");
static const pxr::TfToken fieldIndexToken("fieldIndex");
static const pxr::TfToken densityToken("density");
static const pxr::TfToken velocityToken("velocity");
}

namespace hdMoonray {

using scene_rdl2::logging::Logger;

Volume::Volume(pxr::SdfPath const& id INSTANCERID(const pxr::SdfPath& iid)):
    pxr::HdVolume(id INSTANCERID(iid)), Geometry(this) { }

const std::string&
Volume::className(pxr::HdSceneDelegate* sceneDelegate) const {
    static const std::string r("VdbGeometry");
    return r;
}

void
Volume::Finalize(pxr::HdRenderParam* renderParam)
{
    // std::cout << GetId() << " finalize\n";
    finalize(RenderDelegate::get(renderParam));
}

pxr::HdDirtyBits
Volume::GetInitialDirtyBitsMask() const
{
    return pxr::HdChangeTracker::AllDirty;
}

/// Update the data identified by dirtyBits. Must not query other data.
void
Volume::Sync(pxr::HdSceneDelegate* sceneDelegate,
             pxr::HdRenderParam*   renderParam,
             pxr::HdDirtyBits*     dirtyBits,
             const pxr::TfToken&   reprToken)
{
    const pxr::SdfPath& id = GetId();
    hdmLogSyncStart("Volume", id, dirtyBits);
    
    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));

    if (renderDelegate.getPruneVolume()){
        setPruned(true);
    }else{
        setPruned(false);
    }

    if (pxr::HdChangeTracker::IsVisibilityDirty(*dirtyBits, id))
        _UpdateVisibility(sceneDelegate, dirtyBits);
    Geometry::DirtyPrimvars dirtyPrimvars(getDirtyPrimvars(sceneDelegate, renderDelegate, dirtyBits));

    if (not syncCreateGeometry(sceneDelegate, renderDelegate, dirtyBits, reprToken))
        return;

    // Geometry::DirtyPrimvars dirtyPrimvars(getDirtyPrimvars(sceneDelegate, renderDelegate, dirtyBits)); // unused

    {   UpdateGuard guard(renderDelegate, geometry());
        setCommonAttributes(sceneDelegate, renderDelegate, dirtyBits, dirtyPrimvars);

        if (*dirtyBits & pxr::HdChangeTracker::DirtyVolumeField) {
            auto fields(sceneDelegate->GetVolumeFieldDescriptors(id));
            const std::string* filePath = nullptr;
            for (auto& desc: fields) {
                if (const OpenVdbAsset* const field =
                    dynamic_cast<OpenVdbAsset*>(sceneDelegate->GetRenderIndex().GetBprim(
                                                    desc.fieldPrimType, desc.fieldId))) {
                    if (desc.fieldName == densityToken || desc.fieldName == velocityToken) {
                        const std::string attributeName = desc.fieldName.GetString() + "_grid";
                        const std::string& grid = field->name().GetString();
                        int index = field->index();
                        if (index > 0) {
                            char buf[13]; snprintf(buf, 13, "[%d]", index);
                            geometry()->set(attributeName, grid + buf);
                        } else {
                            geometry()->set(attributeName, grid);
                        }
                    }
                    const std::string& fp = field->filePath();
                    if (not fp.empty()) {
                        if (filePath && *filePath != fp) {
                            Logger::error(id, " all fields must use same vdb file");
                            break;
                        }
                        filePath = &fp;
                    }
                }
                if (filePath) {
                    geometry()->set("model", *filePath);
                }
            }
        }
    }

    // updatePrimvars(dirtyPrimvars, renderDelegate); // there are no primitive_attributes

#if PXR_VERSION >= 2102
    _UpdateInstancer(sceneDelegate, dirtyBits);
#endif
#if PXR_VERSION < 2105
    if (*dirtyBits & pxr::HdChangeTracker::DirtyMaterialId) {
        _SetMaterialId(sceneDelegate->GetRenderIndex().GetChangeTracker(), sceneDelegate->GetMaterialId(id));
    }
#endif
    assign(sceneDelegate, renderDelegate, dirtyBits, true);

    *dirtyBits &= ~pxr::HdChangeTracker::AllSceneDirtyBits;
    hdmLogSyncEnd(id);
}


void
OpenVdbAsset::Sync(pxr::HdSceneDelegate *sceneDelegate,
                   pxr::HdRenderParam   *renderParam,
                   pxr::HdDirtyBits     *dirtyBits)
{
    const pxr::SdfPath& id = GetId();
    // std::cout << id << " Sync dirtyBits=" << std::hex << *dirtyBits << std::endl;
    RenderDelegate::get(renderParam).setStartTime();
    if (*dirtyBits & DirtyParams) {
        pxr::VtValue v;
        v = sceneDelegate->Get(id, pxr::HdFieldTokens->filePath);
        mFilePath = v.Get<pxr::SdfAssetPath>().GetResolvedPath();
        v = sceneDelegate->Get(id, fieldNameToken);
        mName = v.Get<pxr::TfToken>();
        v = sceneDelegate->GetLightParamValue(id, fieldIndexToken);
        mIndex = (not v.IsEmpty()) ? v.Get<int>() : -1;
    }
    *dirtyBits = Clean;
}

}
