// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#define GUESS 1

#include "CoordSys.h"
#include "Camera.h"
#include "GeometryMixin.h"
#include "RenderDelegate.h"
#include "HdmLog.h"

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
    const pxr::SdfPath &id = GetId();
    hdmLogSyncStart("Coordsys", id, dirtyBits);
    *dirtyBits = 0;
    hdmLogSyncEnd(id);
}

scene_rdl2::rdl2::SceneObject*
CoordSys::getBinding(
    pxr::HdSceneDelegate* sceneDelegate, RenderDelegate& renderDelegate,
    const pxr::SdfPath& geomPath, pxr::TfToken name)
{
    // coordsys not implemented
    return nullptr;
}

}

