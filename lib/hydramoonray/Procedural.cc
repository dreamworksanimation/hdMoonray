// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

// Handles any Moonray geometry shader : the RDL class name should be specified by
// procedural:class. Arbitrary RDL2 attribute "xyz" can be set by USD attribute 
// procedural:xyz. primvars:moonray:xyz should also work, and will override procedural:xyz
//
// handles parts, represented by (mis)using GeometrySubset, via ProceduralAdapter.
// Translates primvars to UserData, but only if the RDL class has the
// "primitive_attributes" attribute

#include "Procedural.h"
#include "RenderDelegate.h"
#include "ValueConverter.h"
#include "HdmLog.h"
#include <scene_rdl2/scene/rdl2/Geometry.h>
#include <iostream>

using namespace pxr;

namespace {

    const std::string rdlAttrPrimitiveAttributes("primitive_attributes");

}
namespace hdMoonray {

using PartMaterial = std::pair<SdfPath, SdfPath>;
using PerPartMaterials = std::vector<PartMaterial>;

using scene_rdl2::logging::Logger;

Procedural::Procedural(SdfPath const& id) :
    HdRprim(id), 
    GeometryMixin(this) 
{}

void
Procedural::Finalize(HdRenderParam* renderParam)
{
    // causes geometry to be hidden
    resetGeometryObject(RenderDelegate::get(renderParam));
}

HdDirtyBits
Procedural::GetInitialDirtyBitsMask() const
{
    return HdChangeTracker::AllDirty;
}

// This seems to be a GL thing to set up a shader. Sync is not called.
void
Procedural::_InitRepr(TfToken const &reprToken, HdDirtyBits *dirtyBits)
{
    // doing this seems to cause Sync() to be called:
    *dirtyBits |= HdChangeTracker::DirtyRepr;
}

void
Procedural::syncAttributes(HdSceneDelegate* sceneDelegate, 
                           RenderDelegate& renderDelegate,
                           HdDirtyBits* dirtyBits,
                           const TfToken& reprToken)
{
    // sync all attributes "procedural:xyx" to RDL attr xyz
    const SceneClass& sceneClass = geometry()->getSceneClass();
    for (auto it = sceneClass.beginAttributes(); 
              it != sceneClass.endAttributes(); 
              ++it) {
        const std::string& attrName = (*it)->getName();
        const TfToken primvarName("moonray:"+attrName);
        if (not isPrimvarUsed(primvarName)) {
            const TfToken proceduralName("procedural:"+attrName);
            VtValue val = sceneDelegate->Get(GetId(), proceduralName);
            if (val.IsEmpty()) {
                ValueConverter::setDefault(geometry(), *it);
            } else {
                ValueConverter::setAttribute(geometry(), *it, val);
            }
        }
    }

    // Populate part lists
    partList.clear();
    partMaterials.clear();
    partPaths.clear();
    // ProceduralAdapter will give us a list of parts and materials
    static TfToken partsToken("parts");
    VtValue parts = sceneDelegate->Get(GetId(), partsToken);
    if (parts.IsHolding<PerPartMaterials>()) {
        const PerPartMaterials& ppm = parts.UncheckedGet<PerPartMaterials>();
        partList.reserve(ppm.size());
        partMaterials.reserve(ppm.size());
        partPaths.reserve(ppm.size());
    
        for (const auto& pm : ppm) {
            partList.push_back(pm.first.GetName());
            partPaths.push_back(pm.first);
            partMaterials.push_back(pm.second);
        }
    }

    // base class handles transform and double-sided
    GeometryMixin::syncAttributes(sceneDelegate, renderDelegate, dirtyBits, reprToken);
}

bool
Procedural::supportsUserData() {
    // check if the geometry supports user data, which means it has
    // a "primitive_attributes" attribute
    const SceneClass& sceneClass = geometry()->getSceneClass();
    // there is no hasAttribute(), but getAttribute() throws if the
    // attribute doesn't exist
    try {
        return sceneClass.getAttribute(rdlAttrPrimitiveAttributes) != nullptr;
    } catch (...) {
        return false;
    }
}

void
Procedural::Sync(HdSceneDelegate* sceneDelegate,
                HdRenderParam*   renderParam,
                HdDirtyBits*     dirtyBits,
                const TfToken&   reprToken)
{
    hdmLogSyncStart("Procedural", GetId(), dirtyBits);
    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));

    // compute class name and check if it has changed
    // since this forces a complete resync regardless of incoming
    // dirtyBits, it is a questionable use of the Hydra interface...
    static const TfToken classToken("procedural:class");
    VtValue className(sceneDelegate->Get(GetId(),classToken));
    if (not className.IsHolding<TfToken>()) {
        Logger::error(GetId(), " : requires procedural:class to be set");
        return;
    }
    const std::string sceneClass = className.UncheckedGet<TfToken>().GetString();
    if (geometry() && 
        sceneClass != geometry()->getSceneClass().getName()) {
        // class has changed, need to clear current object
        resetGeometryObject(renderDelegate);
        // force a re-sync of overything
        *dirtyBits = GetInitialDirtyBitsMask();
    }

    _UpdateVisibility(sceneDelegate, dirtyBits);
    _UpdateInstancer(sceneDelegate, dirtyBits);
    
    syncAll(sceneClass, sceneDelegate, renderDelegate, dirtyBits, reprToken);

    hdmLogSyncEnd(GetId());
}

const TfTokenVector&
Procedural::GetBuiltinPrimvarNames() const
{
    static TfTokenVector none{};
    return none;
}

}
