// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RenderSettings.h

#pragma once

#include <pxr/imaging/hd/renderDelegate.h>

#include <string>

namespace hd_render {

class RenderSettings
{
public:
    explicit RenderSettings(pxr::HdRenderDelegate *renderDelegate);
    int setRenderSetting(const std::string &key, const std::string &value);
    std::string printAvailableSettings() const;

private:
    pxr::HdRenderDelegate *mRenderDelegate;
    pxr::HdRenderSettingDescriptorList mRenderSettings;
};

} // namespace hd_render

