// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

// Camera Sprim, implemented by RDL2 PerspectiveCamera
//
// This is complicted by the fact that earlier versions of Moonray did not like changing
// which camera was used after the first render, so this copies all the perspective
// cameras to a single primary camera. This may be fixed now and can be removed.

#include "Camera.h"
#include "GeometryMixin.h"
#include "RenderDelegate.h"
#include "ValueConverter.h"
#include "HdmLog.h"

#include <pxr/imaging/hd/sceneDelegate.h>
#include "pxr/base/gf/frustum.h"
#include "pxr/base/gf/camera.h"

#include <scene_rdl2/scene/rdl2/Camera.h>
#include <scene_rdl2/scene/rdl2/Geometry.h>

#include <iostream>

// define this to avoid switching the identity of the perspective camera
// (see HDM-95/HDM-351)
#define DONT_SWITCH_PERSPECTIVE_CAMERA

namespace {

const pxr::TfToken moonrayClassToken("moonray:class");
const pxr::TfToken PerspectiveCameraToken("PerspectiveCamera");
const pxr::TfToken OrthographicCameraToken("OrthographicCamera");

const hdMoonray::Camera* pCamera = nullptr; // used to detect if camera changed, assume only one global one

// in 2203 DirtyBits::DirtyParams replaced deprecated DirtyProjMatrix
// and DirtyTransform replaced depr DirtyViewMatrix
#if PXR_VERSION >= 2203
const pxr::HdCamera::DirtyBits CamDirtyProj = pxr::HdCamera::DirtyBits::DirtyParams;
const pxr::HdCamera::DirtyBits CamDirtyView = pxr::HdCamera::DirtyBits::DirtyTransform;
#else
const pxr::HdCamera::DirtyBits CamDirtyProj = pxr::HdCamera::DirtyBits::DirtyProjMatrix;
const pxr::HdCamera::DirtyBits CamDirtyView = pxr::HdCamera::DirtyBits::DirtyViewMatrix;
#endif

}

namespace hdMoonray {

using scene_rdl2::logging::Logger;

/// Dirty bits to pass to first call to Sync()
pxr::HdDirtyBits
Camera::GetInitialDirtyBitsMask() const
{
    return DirtyBits::AllDirty;
}

/// Update the data identified by dirtyBits. Must not query other data.
void
Camera::Sync(pxr::HdSceneDelegate* sceneDelegate,
             pxr::HdRenderParam*   renderParam,
             pxr::HdDirtyBits*     dirtyBits)
{
    const pxr::SdfPath &id = GetId();
    hdmLogSyncStart("Camera", id, dirtyBits);

    mSceneDelegate = sceneDelegate; // save for use by setAsPrimaryCamera() and RenderPass
    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));

    // Remember the dirty bits, as HdCamera::Sync clears them
    pxr::HdDirtyBits bits(*dirtyBits);

    // must call this first, it updates members like GetViewMatrix():
    HdCamera::Sync(sceneDelegate, renderParam, dirtyBits);

    // determine the type of camera (orthographic or perspective or custom RDL)
    pxr::TfToken newClass;
    // Use the moonray:class, otherwise guess from the projection matrix (the
    // "projection" attribute is not set by usdview for interactive camera)
    { 
        pxr::VtValue v = sceneDelegate->GetCameraParamValue(GetId(), moonrayClassToken);
        if (v.IsHolding<pxr::TfToken>()) {
            newClass = v.UncheckedGet<pxr::TfToken>();
        } else {

#if PXR_VERSION >= 2203
// in 2203, GetProjectionMatrix() was renamed ComputeProjectionMatrix()
            const auto& matrix = ComputeProjectionMatrix();
#else
            const auto& matrix = GetProjectionMatrix();
#endif
            if (matrix[2][3] == 0) {
                newClass = OrthographicCameraToken;
            } else {
                newClass = PerspectiveCameraToken;
            }
        }
    }

    // The camera is created lazily
    // (rwilson) Is this mutex still needed?
    std::lock_guard<std::mutex> lock(mCreateMutex);
    if (newClass != mClass) {
        mClass = newClass;
        mCamera = nullptr; // this leaks but will be recovered if class goes back to previous value
    } else if (mCamera) {
        updateCamera(sceneDelegate, renderDelegate, bits);
    }

    hdmLogSyncEnd(id);
}

// create the RDL camera if it doesn't exist, and update to match HdCamera
scene_rdl2::rdl2::Camera*
Camera::createCamera(pxr::HdSceneDelegate* sceneDelegate, RenderDelegate& renderDelegate)
{
    if (not mCamera) {
        std::lock_guard<std::mutex> lock(mCreateMutex);
        if (not mCamera) {
            mCamera = renderDelegate.createSceneObject(mClass.GetString(), GetId())->asA<scene_rdl2::rdl2::Camera>();
            updateCamera(sceneDelegate, renderDelegate, DirtyBits::AllDirty);
        }
    }
    return mCamera;
}

// static function to ensure the camera is created at a given path and update it
scene_rdl2::rdl2::Camera*
Camera::createCamera(pxr::HdSceneDelegate* sceneDelegate, RenderDelegate& renderDelegate, const pxr::SdfPath& path)
{
    Camera* camera =
        dynamic_cast<Camera*>(sceneDelegate->GetRenderIndex().GetSprim(pxr::HdPrimTypeTokens->camera, path));
    if (camera) {
        return camera->createCamera(sceneDelegate, renderDelegate);
    }
    return nullptr;
}

// update the existing RDL camera (mCamera) based on the Hydra values and dirty mask
// mCamera must exist when this is called
void
Camera::updateCamera(pxr::HdSceneDelegate* sceneDelegate, RenderDelegate& renderDelegate, pxr::HdDirtyBits bits)
{
    const pxr::SdfPath& id = GetId();
    UpdateGuard guard(renderDelegate, mCamera);

    // update the inverse view matrix (in RDL this is node_xform)
    if (bits & CamDirtyView) {
        pxr::HdTimeSampleArray<pxr::GfMatrix4d, 4> sampledXforms;
        sceneDelegate->SampleTransform(id, &sampledXforms);
        
        if (sampledXforms.count <= 1) {
            // if there's only one sample, it should match the cached value
#if PXR_VERSION >= 2203
            // in 2203, GetViewInverseMatrix was removed, having been previously deprecated
            const pxr::GfMatrix4d xform(GetTransform());
#else
            const pxr::GfMatrix4d xform(GetViewInverseMatrix());
#endif
            mCamera->set(mCamera->sNodeXformKey, reinterpret_cast<const scene_rdl2::rdl2::Mat4d&>(xform));        
        } else {
            // first and last samples will be sample interval boundaries
            pxr::GfCamera cam(sampledXforms.values[0]);
            pxr::GfFrustum frustum = cam.GetFrustum();
            pxr::GfMatrix4d viewInverse = frustum.ComputeViewMatrix().GetInverse();
            mCamera->set(mCamera->sNodeXformKey, reinterpret_cast<const scene_rdl2::rdl2::Mat4d&>(viewInverse));

            cam.SetTransform(sampledXforms.values[sampledXforms.count-1]);
            frustum = cam.GetFrustum();
            viewInverse = frustum.ComputeViewMatrix().GetInverse();
            mCamera->set(mCamera->sNodeXformKey,
                         reinterpret_cast<const scene_rdl2::rdl2::Mat4d&>(viewInverse),
                         scene_rdl2::rdl2::TIMESTEP_END);
        }
        mXformChanged = true;
    }

    // update the projection parameters. HdCamera will calculate the projection matrix
    // for us : from this most of the RDL parameters can be extracted, with the exception
    // of the camera aperture.
    // Note (rwilson) : I'm not sure why the code gets the parameters in this roundabout way,
    // rather that querying them directly, but presumably there was some reason...
    if ((bits & CamDirtyProj) &&
        (mClass == PerspectiveCameraToken || mClass == OrthographicCameraToken)) {
        
        // we compute the following values from attributes and the projection matrix
        // and place them into the RDL camera
        float apertureWidth, apertureHeight;
        float horizOffset, vertOffset;
        float focalLength;
#if PXR_VERSION >= 2203
// in 2203, GetProjectionMatrix() was renamed ComputeProjectionMatrix()
        const auto& matrix = ComputeProjectionMatrix();
#else
        const auto& matrix = GetProjectionMatrix();
#endif

        if (mClass == OrthographicCameraToken) {

            apertureWidth = 2 / matrix[0][0];
            apertureHeight = 2 / matrix[1][1];
            focalLength = 30;
            horizOffset = - apertureWidth * matrix[3][0] / 2;
            vertOffset = - apertureHeight * matrix[3][1] / 2;
            mNear = (matrix[3][2] + 1) / matrix[2][2];
            mFar = (matrix[3][2] - 1) / matrix[2][2];

        } else {

            // The projection matrix cannot be used to obtain the aperture,
            // but once we have the aperture width we can compute the rest
            pxr::VtValue v = sceneDelegate->GetCameraParamValue(id, pxr::HdCameraTokens->horizontalAperture);
            if (v.IsEmpty()) {
                apertureWidth = 20.955f; // USD default value (Moonray default is 24.0)
            } else {
                apertureWidth = v.Get<float>() * 10; // convert back to value in USD prim, Hydra divided by 10
                // HDM-153: Houdini by default measures camera in 1/10 meters. This ran into clamping
                // issues in Moonray (MOONRAY-4261), and may also mess up fstop (below). Check this
            }
            apertureHeight = apertureWidth * matrix[0][0] / matrix[1][1];
            focalLength = apertureWidth * matrix[0][0] / 2;
            horizOffset = apertureWidth * matrix[2][0] / 2;
            vertOffset = apertureHeight * matrix[2][1] / 2;
            mNear = matrix[3][2] / (matrix[2][2] - 1);
            mFar = matrix[3][2] / (matrix[2][2] + 1);
        }

        // apertureWidth/apertureHeight gives us the camera aspect ratio.
        // if necessary, we adjust this based on the requested image aspect ratio (mDesiredAspectRatio)
        // and the window policy. Note that the RDL camera only specifies apertureWidth, so
        // it's not clear that all conform options will work
        pxr::GfVec2d adjustedAperture(apertureWidth, apertureHeight);
        if (mDesiredAspectRatio != 0.0) {
            adjustedAperture = pxr::CameraUtilConformedWindow(adjustedAperture, GetWindowPolicy(), mDesiredAspectRatio);
        }

        // according to MOONRAY-4278, vertical offset needs to be adjusted
        double adjustedAr = adjustedAperture[0]/apertureHeight;
        vertOffset *= adjustedAr;

        mCamera->set("film_width_aperture", (float)adjustedAperture[0]);
        mCamera->setFocalLength(focalLength);
        mCamera->set("horizontal_film_offset", horizOffset);
        mCamera->set("vertical_film_offset", vertOffset);
        mCamera->setNear(mNear);
        mCamera->setFar(mFar);
        mProjChanged = true;
    }

    // handles all other params
    if (bits & (DirtyBits::DirtyParams | CamDirtyProj)) {
        
        pxr::VtValue v;

        // motion params
        if (mClass == PerspectiveCameraToken || mClass == OrthographicCameraToken) {
            v = sceneDelegate->GetCameraParamValue(id, pxr::HdCameraTokens->fStop);
            const float fStop = v.IsEmpty() ? 0.0f : v.Get<float>();
            mCamera->set("dof", fStop != 0.0);
            if (fStop) {
                mCamera->set("dof_aperture", fStop); // Rdl2 "dof_aperture" is actually fStop ratio
                const float focusDistance =
                    sceneDelegate->GetCameraParamValue(id, pxr::HdCameraTokens->focusDistance).Get<float>();
                mCamera->set("dof_focus_distance", focusDistance);
            }
        }

        v = sceneDelegate->GetCameraParamValue(id, pxr::HdCameraTokens->shutterOpen);
        if (not v.IsEmpty()) {
            const float shutterOpen = (v.IsHolding<double>()) ? (v.UncheckedGet<double>()) : v.Get<float>();
            mCamera->set(mCamera->sMbShutterOpenKey, shutterOpen);
        }

        v = sceneDelegate->GetCameraParamValue(id, pxr::HdCameraTokens->shutterClose);
        if (not v.IsEmpty()) {
            const float shutterClose = (v.IsHolding<double>()) ? (v.UncheckedGet<double>()) : v.Get<float>();
            mCamera->set(mCamera->sMbShutterCloseKey, shutterClose);
        }

        // overwrite any attributes with moonray:xyz attributes
        const SceneClass& sceneClass = mCamera->getSceneClass();
        for (auto it = sceneClass.beginAttributes(); it != sceneClass.endAttributes(); ++it) {
            const std::string& attrName = (*it)->getName();

            pxr::VtValue val = sceneDelegate->GetCameraParamValue(id, pxr::TfToken("moonray:"+attrName));
            if (not val.IsEmpty()) {
                ValueConverter::setAttribute(mCamera, *it, val);
            }
            // Fixme: if value is not set it should be reset to default
        }
        mParamsChanged = true;
    }

    // DirtyBits::DirtyWindowPolicy)
    //      The value of GetWindowPolicy() was already updated by HdCamera::Sync()
    // DirtyBits::DirtyClipPlanes) 
    //      arbitrary clipping planes, not supported by Moonray
}

// Set this camera as the camera to use when rendering
//
// This code actually maintains a single RDL perspective camera ("primaryCamera"), and sets this camera as
// active by copying the parameters into "primaryCamera". The original motivation was that changing the
// scene camera identity interactively doesn't work (see HDM-95). This is now addressed, but there are
// some open questions about performance and accuracy of these two approaches (see HDM-351)
// 
// aspectRatio is the shape of the image. The camera's window policy (GetWindowPolicy)
// will be used to adjust the camera as needed to match.
void
Camera::setAsPrimaryCamera(RenderDelegate& renderDelegate, double aspectRatio)
{
    if (aspectRatio != mDesiredAspectRatio) {
        mDesiredAspectRatio = aspectRatio;
        if (mCamera) {
            // will adjusr camera to match mDesiredAspectRatio
            updateCamera(mSceneDelegate, renderDelegate, pxr::HdCamera::DirtyBits::DirtyParams);
        }
    }
    createCamera(mSceneDelegate, renderDelegate);

    scene_rdl2::rdl2::Camera* cameraToUse;

#ifdef DONT_SWITCH_PERSPECTIVE_CAMERA // see HDM-351
    if (mClass == PerspectiveCameraToken) {
        // update single perspective camera in-place
        if (this == pCamera) {
            if (not mXformChanged && not mProjChanged && not mParamsChanged) return;
        } else {
            pCamera = this;
            mXformChanged = mProjChanged = mParamsChanged = true;
        }
        scene_rdl2::rdl2::Camera* primary = cameraToUse = renderDelegate.primaryCamera();
        if (primary == mCamera) return; // this does not happen now, but it might be nice
        UpdateGuard guard(renderDelegate, primary);
        if (mXformChanged) {
            primary->set(mCamera->sNodeXformKey, mCamera->get<scene_rdl2::rdl2::Mat4d>(mCamera->sNodeXformKey));
            primary->set(mCamera->sNodeXformKey,
                         mCamera->get<scene_rdl2::rdl2::Mat4d>(mCamera->sNodeXformKey,scene_rdl2::rdl2::TIMESTEP_END),
                         scene_rdl2::rdl2::TIMESTEP_END);
            mXformChanged = false;
        }
        if (mProjChanged) {
            primary->set("film_width_aperture", mCamera->get<float>("film_width_aperture"));
            primary->set("horizontal_film_offset", mCamera->get<float>("horizontal_film_offset"));
            primary->set("vertical_film_offset", mCamera->get<float>("vertical_film_offset"));
            primary->setFocalLength(mCamera->get<float>("focal"));
            primary->setNear(mNear);
            primary->setFar(mFar);
            mProjChanged = false;
        }
        if (mParamsChanged) {
            primary->set("mb_shutter_open", mCamera->get<float>("mb_shutter_open"));
            primary->set("mb_shutter_close", mCamera->get<float>("mb_shutter_close"));
            primary->set("mb_shutter_bias", mCamera->get<float>("mb_shutter_bias")); // moonray:mb_shutter_bias
            bool dof = mCamera->get<bool>("dof");
            primary->set("dof", dof);
            if (dof) {
                primary->set("dof_aperture", mCamera->get<float>("dof_aperture"));
                primary->set("dof_focus_distance", mCamera->get<float>("dof_focus_distance"));
                // moonray:name attributes:
                primary->set("bokeh", mCamera->get<bool>("bokeh"));
                primary->set("bokeh_angle", mCamera->get<float>("bokeh_angle"));
                primary->set("bokeh_image", mCamera->get<std::string>("bokeh_image"));
                primary->set("bokeh_sides", mCamera->get<int>("bokeh_sides"));
                primary->set("bokeh_weight_location", mCamera->get<float>("bokeh_weight_location"));
                primary->set("bokeh_weight_strength", mCamera->get<float>("bokeh_weight_strength"));
            }
            primary->set("pixel_aspect_ratio", mCamera->get<float>("pixel_aspect_ratio"));
            primary->set("pixel_sample_map", mCamera->get<std::string>("pixel_sample_map"));
            int stereo = mCamera->get<int>("stereo_view");
            primary->set("stereo_view", stereo);
            if (stereo) {
                primary->set("stereo_convergence_distance", mCamera->get<float>("stereo_convergence_distance"));
                primary->set("stereo_interocular_distance", mCamera->get<float>("stereo_interocular_distance"));
            }
            mParamsChanged = false;
        }

    } else

#endif // DONT_SWITCH_PERSPECTIVE_CAMERA
    {
        if (this == pCamera) return;
        pCamera = this;
        cameraToUse = mCamera;
    }
    scene_rdl2::rdl2::SceneVariables& wsv(renderDelegate.acquireSceneContext().getSceneVariables());
    UpdateGuard guard(wsv);
    wsv.set(wsv.sCamera, cameraToUse);
}

std::pair<float, float>
Camera::getTimeSamplingInterval() const
{
    const pxr::SdfPath& id = GetId();
    pxr::HdTimeSampleArray<pxr::GfMatrix4d, 4> sampledXforms;
    mSceneDelegate->SampleTransform(id, &sampledXforms);
    return { sampledXforms.times[0], sampledXforms.times[sampledXforms.count - 1] };
}

void
Camera::Finalize(pxr::HdRenderParam *renderParam)
{
    if (this == pCamera) pCamera = nullptr;
    pxr::HdCamera::Finalize(renderParam);
}

Camera::~Camera()
{
}

}

