// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <tbb/tbb_machine.h> // fix for icc-19/tbb bug
#include <pxr/imaging/hd/camera.h>
namespace scene_rdl2 {namespace rdl2 { class Camera; } }

#include <mutex>

namespace hdMoonray {

class RenderDelegate;

class Camera: public pxr::HdCamera
{
public:
    explicit Camera(const pxr::SdfPath& id): pxr::HdCamera(id) {}

    /// Dirty bits to pass to first call to Sync()
    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    /// Update the data identified by dirtyBits. Must not query other data.
    void Sync(pxr::HdSceneDelegate* sceneDelegate,
              pxr::HdRenderParam*   renderParam,
              pxr::HdDirtyBits*     dirtyBits) override;

    void Finalize(pxr::HdRenderParam *renderParam) override;
    ~Camera();

    scene_rdl2::rdl2::Camera* createCamera(pxr::HdSceneDelegate*, RenderDelegate&);
    static scene_rdl2::rdl2::Camera* createCamera(pxr::HdSceneDelegate*, RenderDelegate&, const pxr::SdfPath&);

    float getNear() const { return mNear; }
    float getFar() const { return mFar; }

    std::pair<float, float> getTimeSamplingInterval() const;

    void setAsPrimaryCamera(RenderDelegate&, double aspectRatio);
    pxr::HdSceneDelegate* getSceneDelegate() const { return mSceneDelegate; }

private:
    pxr::HdSceneDelegate* mSceneDelegate = nullptr; // set by Sync, needed by setAsPrimaryCamera
    void updateCamera(pxr::HdSceneDelegate*, RenderDelegate&, pxr::HdDirtyBits bits);
    scene_rdl2::rdl2::Camera* mCamera = nullptr;
    std::mutex mCreateMutex;
    float mNear = 1;
    float mFar = 10000;

    // set to aspect ratio of image when camera is selected for rendering
    // the aperture ratio of the camera may be adjusted to match by updateCamera()
    double mDesiredAspectRatio = 0;  
    
    pxr::TfToken mClass; // moonray:class
    mutable bool mXformChanged = true;
    mutable bool mProjChanged = true;
    mutable bool mParamsChanged = true;
};

}

