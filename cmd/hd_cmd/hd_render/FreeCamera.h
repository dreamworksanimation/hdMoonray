// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file FreeCamera.h

#pragma once

#include <pxr/base/gf/camera.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>

namespace hd_render {

class FreeCamera
{
    public:
        FreeCamera() : mCamera() {}
        FreeCamera(const pxr::UsdStageRefPtr stage, 
                  float aspectRatio,
                  pxr::UsdTimeCode timeCode, 
                  const pxr::TfTokenVector &includedPurposes);

        pxr::VtValue getParam(pxr::TfToken const& key) const;
        pxr::GfMatrix4d getTransform() const;
        
        const pxr::GfCamera& camera() const { return mCamera; } 
    private:
        pxr::GfCamera mCamera;
        double mShutterOpen = 0;
        double mShutterClose = 0;
        float mExposure = 0;
};


} // namespace hd_render

