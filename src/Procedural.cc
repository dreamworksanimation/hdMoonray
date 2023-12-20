// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Procedural.h"
#include "RenderDelegate.h"
#include "ValueConverter.h"

#include <scene_rdl2/scene/rdl2/Geometry.h>

#include <iostream>

namespace hdMoonray {

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
    // std::cout << id << " Sync dirtyBits=" << std::hex << *dirtyBits << std::endl;
    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));
    renderDelegate.setStartTime();
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
}

const pxr::TfTokenVector&
Procedural::GetBuiltinPrimvarNames() const
{
    static pxr::TfTokenVector none{};
    return none;
}

}
