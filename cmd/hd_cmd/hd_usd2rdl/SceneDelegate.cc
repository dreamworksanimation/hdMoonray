// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file SceneDelegate.cc

#include "SceneDelegate.h"

#include <scene_rdl2/common/platform/Platform.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/usd/usdGeom/tokens.h>

namespace hd_usd2rdl {

SceneDelegate::SceneDelegate(pxr::HdRenderIndex *parentIndex, const pxr::SdfPath &delegateId):
    pxr::HdSceneDelegate(parentIndex, delegateId)
{
}

void
SceneDelegate::populate(const FreeCamera &freeCam, unsigned int width, unsigned int height,
    const pxr::HdRprimCollection &rprimCollection, const pxr::TfTokenVector &purposes,
    const pxr::TfToken &aov, const pxr::HdAovDescriptor &aovDesc)
{
    createFreeCam(freeCam);
    createRenderBuffer(width, height, aovDesc);
    createTask(rprimCollection, purposes, aov, aovDesc, mFreeCamId);
}


void
SceneDelegate::populate(const pxr::SdfPath &cameraId, unsigned int width, unsigned int height,
    const pxr::HdRprimCollection &rprimCollection, const pxr::TfTokenVector &purposes,
    const pxr::TfToken &aov, const pxr::HdAovDescriptor &aovDesc)
{
    createRenderBuffer(width, height, aovDesc);
    createTask(rprimCollection, purposes, aov, aovDesc, cameraId);
}

void
SceneDelegate::createFreeCam(const FreeCamera& freeCam)
{
    mFreeCamId = GetDelegateID().AppendChild(pxr::TfToken("free_cam"));
    GetRenderIndex().InsertSprim(pxr::HdPrimTypeTokens->camera, this, mFreeCamId);
    mFreeCam = freeCam;
    GetRenderIndex().GetChangeTracker().MarkSprimDirty(mFreeCamId, pxr::HdChangeTracker::AllDirty);
}

void
SceneDelegate::createRenderBuffer(unsigned int width, unsigned int height,
    const pxr::HdAovDescriptor &aovDesc)
{
    mRenderBufferId = GetDelegateID().AppendChild(pxr::TfToken("render_buffer"));
    GetRenderIndex().InsertBprim(pxr::HdPrimTypeTokens->renderBuffer, this, mRenderBufferId);
    mRenderBufferDesc = { pxr::GfVec3i(width, height, 1), aovDesc.format, aovDesc.multiSampled };
    GetRenderIndex().GetChangeTracker().MarkBprimDirty(mRenderBufferId, pxr::HdChangeTracker::AllDirty);
}

void
SceneDelegate::createTask(const pxr::HdRprimCollection &rprimCollection, const pxr::TfTokenVector &purposes,
    const pxr::TfToken &aov, const pxr::HdAovDescriptor &aovDesc, const pxr::SdfPath &cameraId)
{
    MNRY_ASSERT_REQUIRE(!mRenderBufferId.IsEmpty()); // call createRenderBuffer() before this method

    mTaskId = GetDelegateID().AppendChild(pxr::TfToken("render_task"));
    GetRenderIndex().InsertTask<pxr::HdxRenderTask>(this, mTaskId);

    // caller should alredy have checked this
    MNRY_ASSERT_REQUIRE(mRenderBufferDesc.format != pxr::HdFormatInvalid);
    pxr::HdRenderPassAovBinding aovBinding;
    aovBinding.renderBufferId = mRenderBufferId;
    aovBinding.aovName = aov;
    aovBinding.clearValue = aovDesc.clearValue;
    pxr::HdxRenderTaskParams params;
    params.aovBindings.push_back(aovBinding);
    params.camera = cameraId;
#if PXR_VERSION >= 2102
    params.framing = pxr::CameraUtilFraming(
        pxr::GfRect2i(pxr::GfVec2i(0,0), mRenderBufferDesc.dimensions[0], mRenderBufferDesc.dimensions[1]));
#else
    params.viewport = pxr::GfVec4f(0, 0, mRenderBufferDesc.dimensions[0], mRenderBufferDesc.dimensions[1]);
#endif
    mTaskParams[pxr::HdTokens->params] = pxr::VtValue(params);
    GetRenderIndex().GetChangeTracker().MarkTaskDirty(mTaskId, pxr::HdChangeTracker::DirtyParams);
    mTaskParams[pxr::HdTokens->collection] = pxr::VtValue(rprimCollection);
    GetRenderIndex().GetChangeTracker().MarkTaskDirty(mTaskId, pxr::HdChangeTracker::DirtyCollection);

    // create the hydra render tags from the included purposes
    // I may not fully understand how this is supposed to work.  If we have only
    // the default_ purpose, then leave the render tags empty.  Otherwise, we
    // should add the geometry tag and any tag that corresponds to render, guide, or proxy.
    // I'm not sure why we need to convert from UsdGeom purpose tags to hydra purpose tags
    mTaskRenderTags.clear();
    MNRY_ASSERT(purposes.size() >= 1 && purposes[0] == pxr::UsdGeomTokens->default_);
    if (purposes.size() > 1) {
        mTaskRenderTags.push_back(pxr::HdRenderTagTokens->geometry);
        for (const pxr::TfToken &p : purposes) {
            if (p == pxr::UsdGeomTokens->render) {
                mTaskRenderTags.push_back(pxr::HdRenderTagTokens->render);
            } else if (p == pxr::UsdGeomTokens->guide) {
                mTaskRenderTags.push_back(pxr::HdRenderTagTokens->guide);
            } else if (p == pxr::UsdGeomTokens->proxy) {
                mTaskRenderTags.push_back(pxr::HdRenderTagTokens->proxy);
            }
        }
    }
}

pxr::VtValue
SceneDelegate::Get(const pxr::SdfPath &id, const pxr::TfToken &key)
{
    if (id == mTaskId) {
        if (mTaskParams.find(key) != mTaskParams.end()) {
            return mTaskParams[key];
        }
    }
    return GetCameraParamValue(id,key);

}

pxr::VtValue
SceneDelegate::GetCameraParamValue(const pxr::SdfPath &id, const pxr::TfToken &key)
{
    if (id == mFreeCamId) {

        pxr::VtValue val = mFreeCam.getParam(key);
        return val;
    }

    return pxr::VtValue();
}

pxr::GfMatrix4d 
SceneDelegate::GetTransform(pxr::SdfPath const & id)
{
    if (id == mFreeCamId) {
        // this is used to get the view inverse matrix in USD 22.3 on
        return mFreeCam.getTransform();
    }
    return pxr::GfMatrix4d();
}

pxr::HdRenderBufferDescriptor
SceneDelegate::GetRenderBufferDescriptor(const pxr::SdfPath &id)
{
    if (id == mRenderBufferId) {
        return mRenderBufferDesc;
    }
    return pxr::HdRenderBufferDescriptor();
}

pxr::TfTokenVector
SceneDelegate::GetTaskRenderTags(const pxr::SdfPath &taskId)
{
    if (taskId == mTaskId) {
        return mTaskRenderTags;
    }

    // maybe the base class knows?
    return pxr::HdSceneDelegate::GetTaskRenderTags(taskId);
}

} // namespace hd_usd2rdl

