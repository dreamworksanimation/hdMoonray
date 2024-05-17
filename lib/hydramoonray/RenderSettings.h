// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Renderer.h"
#include <pxr/imaging/hd/renderDelegate.h>

namespace hdMoonray {

class RenderDelegate;


// Render settings are stored generically by the RenderDelegate. The job of this class is
// (a) to know which render settings moonray supports (getDescriptors)
// (b) to take the current values stored by RenderDelegate and apply
//     them to moonray specific scene data (apply). Generally this means modifying the
//     SceneVariables object.
class RenderSettings
{
public:
    RenderSettings(RenderDelegate& delegate) : mDelegate(delegate) {}
    
    void addDescriptors(pxr::HdRenderSettingDescriptorList& descriptorList) const;

    // if stored settings have changed since the last apply,
    // apply them to the scene. SceneContext must exist, and may be acquired
    void apply();

    // get a render setting
    pxr::VtValue getRenderSetting(const pxr::TfToken& key) const;
    template<typename T> T get(const pxr::TfToken& key) const {
        return getRenderSetting(key).Get<T>();
    }

    std::string getExecutionMode() const;

private:

    RenderDelegate& mDelegate;
};

}
