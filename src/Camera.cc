// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

// Camera Sprim, implemented by RDL2 PerspectiveCamera
//
// This is complicted by the fact that earlier versions of Moonray did not like changing
// which camera was used after the first render, so this copies all the perspective
// cameras to a single primary camera. This may be fixed now and can be removed.

#include "Camera.h"
#include "Geometry.h"
#include "RenderDelegate.h"
#include "ValueConverter.h"

#include <pxr/imaging/hd/sceneDelegate.h>
#include "pxr/base/gf/frustum.h"
#include "pxr/base/gf/camera.h"

#include <scene_rdl2/scene/rdl2/Camera.h>
#include <scene_rdl2/scene/rdl2/Geometry.h>

#include <iostream>

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
    // std::cout << GetId() << ": Sync" << std::endl;
    mSceneDelegate = sceneDelegate; // save for use by setAsPrimaryCamera() and RenderPass
    RenderDelegate& renderDelegate(RenderDelegate::get(renderParam));
    renderDelegate.setStartTime();

    // Remember the dirty bits as HdCamera::Sync clears them
    pxr::HdDirtyBits bits(*dirtyBits);

    // must call this first, it updates members like GetViewMatrix():
    HdCamera::Sync(sceneDelegate, renderParam, dirtyBits);

    pxr::TfToken newClass;
    // Use the moonray:class, otherwise guess from the projection matrix (the
    // "projection" attriubute is not set by usdview for interactive camera)
    { pxr::VtValue v = sceneDelegate->GetCameraParamValue(GetId(), moonrayClassToken);
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

    std::lock_guard<std::mutex> lock(mCreateMutex);
    if (newClass != mClass) {
        mClass = newClass;
        mCamera = nullptr; // this leaks but will be recovered if class goes back to previous value
    } else if (mCamera) {
        updateCamera(sceneDelegate, renderDelegate, bits);
    }
}

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

void
Camera::updateCamera(pxr::HdSceneDelegate* sceneDelegate, RenderDelegate& renderDelegate, pxr::HdDirtyBits bits)
{
    const pxr::SdfPath& id = GetId();
    UpdateGuard guard(renderDelegate, mCamera);

    if (bits & CamDirtyView) {
        pxr::HdTimeSampleArray<pxr::GfMatrix4d, 4> sampledXforms;
        sceneDelegate->SampleTransform(id, &sampledXforms);
        // if there's only one sample, it should match the cached value
        if (sampledXforms.count <= 1) {
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

    if ((bits & CamDirtyProj) &&
        (mClass == PerspectiveCameraToken || mClass == OrthographicCameraToken)) {
        float w, h, cx, cy, fl;

        // inverse of calculation in base/gf/frustum.cpp
#if PXR_VERSION >= 2203
// in 2203, GetProjectionMatrix() was renamed ComputeProjectionMatrix()
        const auto& matrix = ComputeProjectionMatrix();
#else
        const auto& matrix = GetProjectionMatrix();
#endif
        if (mClass == OrthographicCameraToken) {
            w = 2 / matrix[0][0];
            h = 2 / matrix[1][1];
            fl = 30;
            cx = - w * matrix[3][0] / 2;
            cy = - h * matrix[3][1] / 2;
            mNear = (matrix[3][2] + 1) / matrix[2][2];
            mFar = (matrix[3][2] - 1) / matrix[2][2];
        } else {
            // Aperture width cannot be determined from matrix, read the attribute
            pxr::VtValue v = sceneDelegate->GetCameraParamValue(id, pxr::HdCameraTokens->horizontalAperture);
            if (v.IsEmpty()) {
                w = 20.955f; // USD default value (Moonray default is 24.0)
            } else {
                w = v.Get<float>() * 10; // convert back to value in USD prim, Hydra divided by 10
                // HDM-153: Houdini by default measures camera in 1/10 meters. This ran into clamping
                // issues in Moonray (MOONRAY-4261), and may also mess up fstop (below). Check this
            }
            h = w * matrix[0][0] / matrix[1][1];
            fl = w * matrix[0][0] / 2;
            cx = w * matrix[2][0] / 2;
            cy = h * matrix[2][1] / 2;
            mNear = matrix[3][2] / (matrix[2][2] - 1);
            mFar = matrix[3][2] / (matrix[2][2] + 1);
        }

        if (mAspectRatio != 0.0) {
            w = pxr::CameraUtilConformedWindow(pxr::GfVec2d(w, h), GetWindowPolicy(), mAspectRatio)[0];
        }

        mCamera->set("film_width_aperture", w);
        mCamera->setFocalLength(fl);
        mCamera->set("horizontal_film_offset", cx);
        mCamera->set("vertical_film_offset", cy * w / h); // MOONRAY-4278, cy units are wrong
        mCamera->setNear(mNear);
        mCamera->setFar(mFar);
        mProjChanged = true;
    }

    if (bits & (DirtyBits::DirtyParams | CamDirtyProj)) {
        pxr::VtValue v;
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

                if (attrName == "geometry") {
                    // GetCameraParam cannot read 'rel' properties, so
                    // geometry for a BakeCamera must be defined as a string
                    if (val.IsHolding<pxr::SdfPath>()) {
                        pxr::SdfPath geomPath = val.UncheckedGet<pxr::SdfPath>();
                        // ! not safe, because geom may not use the same delegate
                        geomPath.ReplacePrefix(pxr::SdfPath::AbsoluteRootPath(), sceneDelegate->GetDelegateID());
                        SceneObject* so = Geometry::createGeometry(sceneDelegate, renderDelegate, geomPath);
                        mCamera->set("geometry", so);
                        if (not so) {
                            Logger::error(GetId(), ".geometry: ", geomPath, " not found");
                        }
                        continue;
                    }
                }

                ValueConverter::setAttribute(mCamera, *it, val);
            }
            // Fixme: if value is not set it should be reset to default
        }
        mParamsChanged = true;
    }

    if (bits & DirtyBits::DirtyWindowPolicy) {
        // The value of GetWindowPolicy() was already updated by HdCamera::Sync()
    }

    if (bits & DirtyBits::DirtyClipPlanes) {
        // arbitrary clipping planes, not supported by Moonray
    }
}

// aspectRatio is the shape of the image. This is needed to implement GetWindwowPolicy, it will zoom
// the camera in/out as necessary because moonray only implments MatchHorizontally policy
void
Camera::setAsPrimaryCamera(RenderDelegate& renderDelegate, double aspectRatio)
{
    if (aspectRatio != mAspectRatio) {
        mAspectRatio = aspectRatio;
        if (mCamera) {
            updateCamera(mSceneDelegate, renderDelegate, CamDirtyProj);
        }
    }
    createCamera(mSceneDelegate, renderDelegate);

    scene_rdl2::rdl2::Camera* cameraToUse;
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

    } else {
        // switch to any other class of camera
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

