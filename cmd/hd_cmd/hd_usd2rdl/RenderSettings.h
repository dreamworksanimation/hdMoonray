// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RenderSettings.h

#pragma once

#include <tbb/tbb_machine.h> // fix for icc-19/tbb bug
#include <pxr/imaging/hd/renderDelegate.h>

#include <string>

namespace hd_usd2rdl {

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

