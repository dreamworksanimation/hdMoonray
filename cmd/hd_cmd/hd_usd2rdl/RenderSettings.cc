// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RenderSettings.cc

#include "RenderSettings.h"

#include <iostream>
#include <sstream>

namespace hd_usd2rdl {

RenderSettings::RenderSettings(pxr::HdRenderDelegate *renderDelegate):
    mRenderDelegate(renderDelegate)
{
    mRenderSettings = mRenderDelegate->GetRenderSettingDescriptors();
}

int
RenderSettings::setRenderSetting(const std::string &key, const std::string &value)
{
    // Probably should figure out the data type from SceneVariable Attribute, but
    // this version uses the text to guess at the type.
    pxr::VtValue v;
    char* endptr = nullptr;
    if (value.empty())
        return -1;
    else if (value.find('.') != value.npos)
        v = strtof(value.c_str(), &endptr);
    else if (not strcasecmp(value.c_str(), "false"))
        v = false;
    else if (not strcasecmp(value.c_str(), "true"))
        v = true;
    else
        v = int(strtol(value.c_str(), &endptr, 0));

    if (endptr && *endptr) v = value; // text after the number

    if (!v.IsEmpty()) {
        mRenderDelegate->SetRenderSetting(pxr::TfToken(key), v);
        return 0;
    }

    return -1;  // probably hit a type we don't know how to parse
}

std::string
RenderSettings::printAvailableSettings() const
{
    std::ostringstream ostr;
    for (const pxr::HdRenderSettingDescriptor &desc : mRenderSettings) {
        ostr << '\t' << "\"" << desc.key << "\" [default = "
             << desc.defaultValue << "] (type = "
             << desc.defaultValue.GetType() << ")\n";
    }
    ostr << "\t\"sceneVariable:xxxx\" Use 'rdl2_print SceneVariables' for a list.\n";
    return ostr.str();
}

} // namespace hd_render

