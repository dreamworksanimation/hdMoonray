// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file SceneDelegate.h

#pragma once

#include "FreeCamera.h"
#include <tbb/tbb_machine.h> // fix for icc-19/tbb bug
#include <pxr/imaging/hd/sceneDelegate.h>
namespace hd_usd2rdl {

// A scene delegate class that manages application
// specific scene data such as render tasks, free cameras, and
// render buffers
class SceneDelegate : public pxr::HdSceneDelegate
{
public:
    SceneDelegate(pxr::HdRenderIndex *parentIndex, const pxr::SdfPath &delegateId);

    // Create a scene with a task that renders rprimCollection from the camera 
    // into a width x height viewport
    void populate(const FreeCamera &camera, unsigned int width, unsigned int height,
        const pxr::HdRprimCollection &rprimCollection, const pxr::TfTokenVector &purposes,
        const pxr::TfToken &aov, const pxr::HdAovDescriptor &aovDesc);

    // Create a scene with a task that renders rprimCollection from cameraId
    // into a width x height viewport.  The caller is responsible for ensuring
    // that cameraId references a valid camera.
    void populate(const pxr::SdfPath &cameraId, unsigned int width, unsigned int height,
        const pxr::HdRprimCollection &rprimCollection, const pxr::TfTokenVector &purposes,
        const pxr::TfToken &aov, const pxr::HdAovDescriptor &aovDesc);

    const pxr::SdfPath &getFreeCamId() const { return mFreeCamId; }

    const pxr::SdfPath &getRenderBufferId() const { return mRenderBufferId; }
    unsigned int getRenderBufferWidth() const { return mRenderBufferDesc.dimensions[0]; }
    unsigned int getRenderBufferHeight() const { return mRenderBufferDesc.dimensions[1]; }
    pxr::HdFormat getRenderBufferFormat() const { return mRenderBufferDesc.format; }

    const pxr::SdfPath &getTaskId() const { return mTaskId; }

    // HdSceneDelegate Overrides
    pxr::VtValue Get(const pxr::SdfPath &id, const pxr::TfToken &key) override;
    pxr::VtValue GetCameraParamValue(const pxr::SdfPath &id, const pxr::TfToken &key) override;
    pxr::HdRenderBufferDescriptor GetRenderBufferDescriptor(const pxr::SdfPath &id) override;
    pxr::TfTokenVector GetTaskRenderTags(const pxr::SdfPath &taskId) override;
    pxr::GfMatrix4d GetTransform(pxr::SdfPath const & id) override;

private:
    void createFreeCam(const FreeCamera &camera);
    void createRenderBuffer(unsigned int width, unsigned int height,
        const pxr::HdAovDescriptor &aovDesc);
    void createTask(const pxr::HdRprimCollection &rprimCollection, const pxr::TfTokenVector &purposes,
        const pxr::TfToken &aov, const pxr::HdAovDescriptor &aovDesc, const pxr::SdfPath &cameraId);

    // assumes a single render task
    pxr::SdfPath mTaskId;
    pxr::VtDictionary mTaskParams;
    pxr::TfTokenVector mTaskRenderTags;

    // assumes a single free camera
    pxr::SdfPath mFreeCamId;
    FreeCamera mFreeCam;

    // assumes a single render buffer
    pxr::SdfPath mRenderBufferId;
    pxr::HdRenderBufferDescriptor mRenderBufferDesc;
};

} // namespace hd_usd2rdl

