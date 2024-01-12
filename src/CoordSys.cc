// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#define GUESS 1

#include "CoordSys.h"
#include "Camera.h"
#include "Geometry.h"
#include "RenderDelegate.h"

#include <pxr/imaging/hd/sceneDelegate.h>
#include <scene_rdl2/scene/rdl2/Geometry.h>
#include <scene_rdl2/scene/rdl2/Camera.h>

namespace {

bool
match(const pxr::SdfPath& path, const pxr::TfToken& name)
{
    // Instancing may add "proto_" before and "_id#" after the name.
    // Also DWA data may have arbitrary prefixes before the name but also with an underscore
    // So match exactly, or with an underscore before it and optional "_id" after it.
    if (path.GetNameToken() == name) return true;
    const std::string& pathname = path.GetName();
    size_t pos = pathname.find(name);
    if (pos > 1 && pathname[pos-1] == '_') {
        size_t e = pos + name.GetString().size();
        if (e == pathname.size() ||
            (pathname[e] == '_' && pathname[e + 1] == 'i' && pathname[e + 2] == 'd'))
            return true;
    }
    return false;
}

const pxr::TfToken GeometryToken("Geometry");
const pxr::TfToken LooksToken("Looks");

}

namespace hdMoonray {

using scene_rdl2::logging::Logger;

CoordSys::CoordSys(pxr::SdfPath const& id): pxr::HdCoordSys(id) {}

void
CoordSys::Sync(pxr::HdSceneDelegate* sceneDelegate,
               pxr::HdRenderParam*   renderParam,
               pxr::HdDirtyBits*     dirtyBits)
{
    RenderDelegate::get(renderParam).setStartTime();
    // HdCoordSys::Sync(sceneDelegate, renderParam, dirtyBits); // this does not exist!
    *dirtyBits = 0;
}

scene_rdl2::rdl2::SceneObject*
CoordSys::getBinding(
    pxr::HdSceneDelegate* sceneDelegate, RenderDelegate& renderDelegate,
    const pxr::SdfPath& geomPath, pxr::TfToken name)
{
#if 0
    // For reference, this is how to find the CoordSys object. This does not work for
    // instanced objects (may be fixed in Hydra for usd-21). Also it is unclear what
    // coordSys->getBinding() (which is nyi) should do as it only has the affine transform
    // available, no projections and it does not have the name of the linked object.
    // A dummy object could be created, or this function's api altered to return
    // the projection so it can be directly stored in the shader. Projectors may still
    // require the alternative "guess" code.
    pxr::HdIdVectorSharedPtr bindings(sceneDelegate->GetCoordSysBindings(geomPath));
    if (bindings) {
        for (const pxr::SdfPath& path : *bindings) {
            auto coordSys = static_cast<hdMoonray::CoordSys*>(
                sceneDelegate->GetRenderIndex().GetSprim(pxr::HdPrimTypeTokens->coordSys, path));
            if (coordSys && coordSys->GetName() == name) {
                return coordSys->getBinding(sceneDelegate, renderDelegate);
            }
        }
    }

#else // "guess" at the linked object. Very fragile especially for instanced geometry

    // Back up to parent of "Geometry" or "Looks"
    pxr::SdfPath base = geomPath.GetParentPath();
    for (;;) {
        if (base.GetPathElementCount() < 2) {
            base = geomPath.GetParentPath();
            break;
        }
        pxr::TfToken nameToken = base.GetNameToken();
        base = base.GetParentPath();
        if (nameToken == GeometryToken || nameToken == LooksToken) break;
    }

    // search for Cameras
    pxr::SdfPathVector sprims =
        sceneDelegate->GetRenderIndex().GetSprimSubtree(pxr::HdPrimTypeTokens->camera, base);
    for (auto&& path : sprims) {
        if (match(path, name)) {
            scene_rdl2::rdl2::SceneObject* sceneObject =
                Camera::createCamera(sceneDelegate, renderDelegate, path);
            if (sceneObject) return sceneObject;
        }
    }

    // search for Geometry objects, also for xform parents of Geometry
    pxr::SdfPathVector rprims = sceneDelegate->GetRenderIndex().GetRprimSubtree(base);
    for (auto&& path : rprims) {
        if (match(path, name) || match(path.GetParentPath(), name)) {
            scene_rdl2::rdl2::SceneObject* sceneObject =
                Geometry::createGeometry(sceneDelegate, renderDelegate, path);
            if (sceneObject) return sceneObject;
        }
    }
#endif

    return nullptr;
}

}

