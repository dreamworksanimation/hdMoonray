// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Procedural.h"
#include "RenderDelegate.h"
#include "ValueConverter.h"
#include "HdmLog.h"

#include <scene_rdl2/scene/rdl2/Geometry.h>

#include <iostream>

namespace hdMoonray {

static pxr::TfToken partsToken("parts");

using PartMaterial = std::pair<pxr::SdfPath, pxr::SdfPath>;
using PerPartMaterials = std::vector<PartMaterial>;

using scene_rdl2::logging::Logger;

Procedural::Procedural(pxr::SdfPath const& id INSTANCERID(const pxr::SdfPath& iid)):
    pxr::HdRprim(id INSTANCERID(iid)), Geometry(this) {}

const std::string&
Procedural::className(pxr::HdSceneDelegate* sceneDelegate) const
{
    static const pxr::TfToken classToken("procedural:class");
    pxr::VtValue v(get(classToken, sceneDelegate));
    if (v.IsHolding<pxr::TfToken>()) {
        const pxr::TfToken& sceneClass = v.Get<pxr::TfToken>();
        return sceneClass.GetString();
    } else {
        Logger::error(GetId(), ": missing procedural:class");
        static const std::string BoxGeometry("BoxGeometry");
        return BoxGeometry;
    }
}

void
Procedural::Finalize(pxr::HdRenderParam* renderParam)
{
    // std::cout << GetId() << " finalize\n";
    finalize(RenderDelegate::get(renderParam));
}

pxr::HdDirtyBits
Procedural::GetInitialDirtyBitsMask() const
{
    return pxr::HdChangeTracker::AllDirty;
}

// This seems to be a GL thing to set up a shader. Sync is not called.
void
Procedural::_InitRepr(pxr::TfToken const &reprToken, pxr::HdDirtyBits *dirtyBits)
{
    // doing this seems to cause Sync() to be called:
    *dirtyBits |= pxr::HdChangeTracker::DirtyRepr;
}

void
Procedural::setPruneFlag(RenderDelegate& renderDelegate,
                         pxr::HdSceneDelegate* sceneDelegate){
    if (renderDelegate.getPruneWillow() && className(sceneDelegate) == "WillowGeometry_v3"){
        setPruned(true);
    }
    else if (renderDelegate.getPruneFurDeform() && className(sceneDelegate) == "FurDeformGeometry"){
        setPruned(true);
    }
    else if (renderDelegate.getPruneCurveDeform() && className(sceneDelegate)== "CurveDeformGeometry"){
        setPruned(true);
    }
    else if (renderDelegate.getPruneWrapDeform() && className(sceneDelegate) == "WrapDeformGeometry"){
        setPruned(true);
    }
    else{
        setPruned(false);
    }
}

/// Update the data identified by dirtyBits. Must not query other data.
void
Procedural::Sync(pxr::HdSceneDelegate* sceneDelegate,
             pxr::HdRenderParam*   renderParam,
             pxr::HdDirtyBits*     dirtyBits,
             const pxr::TfToken&   reprToken)
{
    const pxr::SdfPath& id = GetId();
    hdmLogSyncStart("Procedural", id, dirtyBits);

    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));

    setPruneFlag(renderDelegate, sceneDelegate);

    if (pxr::HdChangeTracker::IsVisibilityDirty(*dirtyBits, id))
        _UpdateVisibility(sceneDelegate, dirtyBits);
    Geometry::DirtyPrimvars dirtyPrimvars(getDirtyPrimvars(sceneDelegate, renderDelegate, dirtyBits));

    if (not geometry()) {
        if (not syncCreateGeometry(sceneDelegate, renderDelegate, dirtyBits, reprToken))
            return;
    }

    {   UpdateGuard guard(renderDelegate, geometry());
        setCommonAttributes(sceneDelegate, renderDelegate, dirtyBits, dirtyPrimvars);

        // copy all the procedural: attributes
        const scene_rdl2::rdl2::SceneClass& sceneClass = geometry()->getSceneClass();
        for (auto it = sceneClass.beginAttributes(); it != sceneClass.endAttributes(); ++it) {
            const std::string& attrName = (*it)->getName();
            if (attrName == "node_xform") continue; // use Usd xform only
            if (not attrName.compare(0, 8, "visible_")) continue; // these are done by setVisible()
            if (attrName == "primitive_attributes") hasPrimitiveAttributes = true;
            pxr::VtValue val = get(pxr::TfToken("procedural:"+attrName), sceneDelegate);
            if (!val.IsEmpty()) {
                ValueConverter::setAttribute(geometry(),*it,val);
            } else {
                ValueConverter::setDefault(geometry(), *it);
            }
        }
    }

    if (hasPrimitiveAttributes) { // this crashes if no primitive_attributes attribute
        updatePrimvars(dirtyPrimvars, renderDelegate);
    }

    // Populate part lists (base Geometry class will perform actual assignment)
    partList.clear();
    partMaterials.clear();
    partPaths.clear();
    // ProceduralAdapter will give us a list of parts and materials
    pxr::VtValue parts = sceneDelegate->Get(id,partsToken);
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

#if PXR_VERSION >= 2102
    _UpdateInstancer(sceneDelegate, dirtyBits);
#endif
#if PXR_VERSION < 2105
    if (*dirtyBits & pxr::HdChangeTracker::DirtyMaterialId) {
        _SetMaterialId(sceneDelegate->GetRenderIndex().GetChangeTracker(), sceneDelegate->GetMaterialId(id));
    }
#endif
    assign(sceneDelegate, renderDelegate, dirtyBits,
           not strncmp(geometry()->getSceneClass().getName().c_str(), "OpenVdb", 7));

    *dirtyBits &= ~pxr::HdChangeTracker::AllSceneDirtyBits;
    hdmLogSyncEnd(id);
}

const pxr::TfTokenVector&
Procedural::GetBuiltinPrimvarNames() const
{
    static pxr::TfTokenVector none{};
    return none;
}

}
