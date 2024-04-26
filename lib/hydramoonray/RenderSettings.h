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
    static const pxr::HdRenderSettingDescriptorList& getDescriptors();

    RenderSettings(RenderDelegate& delegate) : mDelegate(delegate) {}

    // if stored settings have changed since the last apply,
    // apply them to the scene. SceneContext must exist, and may be acquired
    void apply();

    bool getArrasLocalMode() const;
    int getArrasLogLevel() const;
    int getArrasHostCount() const;
    int getArrasLocalReservedCores() const;
    bool getRestartToggle() const;
    bool getReloadTexturesToggle() const;
    float getArrasMaxFps() const;
    int getMaxConnectRetries() const;
    bool enableDenoise() const;
    bool denoiseAlbedoGuiding() const;
    bool denoiseNormalGuiding() const;
    bool getDecodeNormals() const;
    bool getEnableMotionBlur() const;
    std::string getExecutionMode() const;
    bool mIsHoudini = false;

    // utility function for settings stored in environment variables:
    static const char* getenvString(const char* name, const char* dflt = 0);

private:
    template<typename T> T get(const pxr::TfToken& key) const;
    RenderDelegate& mDelegate;
};

}
