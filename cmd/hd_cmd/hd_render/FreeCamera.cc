// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file FreeCamera.cc

#include "FreeCamera.h"

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/gf/vec2d.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/imaging/hd/camera.h>

#include <iostream>
#include <math.h>

namespace hd_render {

FreeCamera::FreeCamera(const pxr::UsdStageRefPtr stage, 
                       float aspectRatio,
                        pxr::UsdTimeCode timeCode, 
                        const pxr::TfTokenVector &includedPurposes)
{
    // Generate a camera that defines a decent default view of the stage's
    // renderable primitives.
    //
    // This is mostly lifted from pxr/usdImaging/usdAppUtils/frameRecorder.cpp so
    // it should closely match the default view in tools such as usdview.
    //
    // The main difference is that we lock the vertical aperture so the
    // render window aspect matches the viewport.

    // Start with a default (50mm) perspective GfCamera.
    // Set the vertical aperture based on the viewport aspectRatio (height / width)
    mCamera.SetVerticalAperture(mCamera.GetHorizontalAperture() * aspectRatio);
    pxr::UsdGeomBBoxCache bboxCache(timeCode, includedPurposes,
        /* useExtentsHint = */ true);
    pxr::GfBBox3d bbox = bboxCache.ComputeWorldBound(stage->GetPseudoRoot());
    pxr::GfVec3d center = bbox.ComputeCentroid();
    pxr::GfRange3d range = bbox.ComputeAlignedRange();
    pxr::GfVec3d dim = range.GetSize();
    pxr::TfToken upAxis = pxr::UsdGeomGetStageUpAxis(stage);
    // Find corner of bbox in the focal plane.
    pxr::GfVec2d planeCorner;
    if (upAxis == pxr::UsdGeomTokens->y) {
        planeCorner = pxr::GfVec2d(dim[0], dim[1]) / 2;
    } else {
        planeCorner = pxr::GfVec2d(dim[0], dim[2]) / 2;
    }
    float planeRadius = sqrt(pxr::GfDot(planeCorner, planeCorner));
    // Compute distance to focal plane.
    float halfFov = mCamera.GetFieldOfView(pxr::GfCamera::FOVHorizontal) / 2.0;
    float distance = planeRadius / tan(pxr::GfDegreesToRadians(halfFov));
    // Back up to frame the front face of the bbox.
    if (upAxis == pxr::UsdGeomTokens->y) {
        distance += dim[2] / 2;
    } else {
        distance += dim[1] / 2;
    }
    // Compute local-to-world transform for camera filmback.
    pxr::GfMatrix4d xf;
    if (upAxis == pxr::UsdGeomTokens->y) {
        xf.SetTranslate(center + pxr::GfVec3d(0, 0, distance));
    } else {
        xf.SetRotate(pxr::GfRotation(pxr::GfVec3d(1, 0, 0), 90));
        xf.SetTranslateOnly(center + pxr::GfVec3d(0, -distance, 0));
    }
    mCamera.SetTransform(xf);
}

pxr::VtValue 
FreeCamera::getParam(pxr::TfToken const& key) const
{
#if PXR_VERSION >= 2108
     if (key == pxr::HdCameraTokens->projection) {
        if (mCamera.GetProjection() == pxr::GfCamera::Projection::Orthographic) {
            return pxr::VtValue(pxr::HdCamera::Orthographic);
        }
        return pxr::VtValue(pxr::HdCamera::Perspective);
    } else if (key == pxr::HdCameraTokens->horizontalAperture) {
        // Several camera parameters are specified in tenths of a
        // world unit (e.g., focalLength = 50mm)
        // Hydra's camera expects these parameters to be expressed in world
        // units. (e.g., if cm is the world unit, focalLength = 5cm)
        return pxr::VtValue(mCamera.GetHorizontalAperture() * float(pxr::GfCamera::APERTURE_UNIT));
    } else if (key == pxr::HdCameraTokens->verticalAperture) { 
        return pxr::VtValue(mCamera.GetVerticalAperture() * float(pxr::GfCamera::APERTURE_UNIT));
    } else if (key == pxr::HdCameraTokens->horizontalApertureOffset) {
        return pxr::VtValue(mCamera.GetHorizontalApertureOffset() * float(pxr::GfCamera::APERTURE_UNIT));
    } else if (key == pxr::HdCameraTokens->verticalApertureOffset) {
        return pxr::VtValue(mCamera.GetVerticalApertureOffset() * float(pxr::GfCamera::APERTURE_UNIT));
    } else if (key == pxr::HdCameraTokens->focalLength) {
        return pxr::VtValue(mCamera.GetFocalLength() * float(pxr::GfCamera::FOCAL_LENGTH_UNIT));
    } else if (key == pxr::HdCameraTokens->clippingRange) {
        return pxr::VtValue(mCamera.GetClippingRange());
    } else if (key == pxr::HdCameraTokens->clipPlanes) {
        // GfCamera clip planes are vector<GfVec4f> but Hydra
        // expects vector<GfVec4d>
        const std::vector<pxr::GfVec4f>& fPlanes = mCamera.GetClippingPlanes();
        std::vector<pxr::GfVec4d> dPlanes;
        dPlanes.reserve(fPlanes.size());
        dPlanes.assign(fPlanes.cbegin(), fPlanes.cend());
        return pxr::VtValue(dPlanes);
    } else if (key == pxr::HdCameraTokens->fStop) {
        return pxr::VtValue(mCamera.GetFStop());
    } else if (key == pxr::HdCameraTokens->focusDistance) {
        return pxr::VtValue((mCamera.GetFocusDistance()));
    } else if (key == pxr::HdCameraTokens->shutterOpen) {
        return pxr::VtValue(mShutterOpen);
    } else if (key == pxr::HdCameraTokens->shutterClose) {
        return pxr::VtValue(mShutterClose);
    } else if (key == pxr::HdCameraTokens->exposure) {
        return pxr::VtValue(mExposure);
    } else 
#endif   
    if (key == pxr::TfToken("worldToViewMatrix")) {
        // only used pre-0.22.3
        return pxr::VtValue(mCamera.GetFrustum().ComputeViewMatrix());
    } else if (key == pxr::TfToken("projectionMatrix")) {
        // only used pre-0.22.3
        return pxr::VtValue(mCamera.GetFrustum().ComputeProjectionMatrix());
    } else if (key == pxr::HdCameraTokens->windowPolicy) {
        return pxr::VtValue(pxr::CameraUtilFit);
    }

    return pxr::VtValue();
}
pxr::GfMatrix4d 
FreeCamera::getTransform() const
{
    return mCamera.GetFrustum().ComputeViewMatrix().GetInverse();
}

} // namespace hd_render

