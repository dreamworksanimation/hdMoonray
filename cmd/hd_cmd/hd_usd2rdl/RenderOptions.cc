// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RenderOptions.cc

#include "RenderOptions.h"

#include <scene_rdl2/render/util/Args.h>
#include <scene_rdl2/render/util/Strings.h>

#include <stdlib.h>
#include <string>
#include <vector>

namespace hd_usd2rdl {

// default values
const bool RenderOptions::sHelpRequested = false;

const char * const RenderOptions::sInputSceneFile = "scene.usd";
const char * const RenderOptions::sOutputRdlFile = "scene.rdl";

const char * const RenderOptions::sCamera = "";

const unsigned int RenderOptions::sWidth = 1920;
const unsigned int RenderOptions::sHeight = 1080;
const unsigned int RenderOptions::sRefineLevel = 1; // "Complexity/Medium"
const TimeType RenderOptions::sTimeType = TimeType::Earliest;

const bool RenderOptions::sPrintRenderSettingsRequested = false;

RenderOptions::RenderOptions(int argc, char *argv[]):
    // initialize defaults
    mHelpRequested(sHelpRequested),
    mInputSceneFile(sInputSceneFile),
    mOutputRdlFile(sOutputRdlFile),
    mCamera(sCamera),
    mWidth(sWidth),
    mHeight(sHeight),
    mRefineLevel(sRefineLevel),
    mTimeType(sTimeType),
    mPrintRenderSettingsRequested(sPrintRenderSettingsRequested),
    mAllFlagsValid(true)
{
    using scene_rdl2::util::Args;

    Args args(argc, argv);
    Args::StringArray values;
    std::vector<std::string> validFlags;

    validFlags.push_back("-h");
    validFlags.push_back("-help");
    if (argc <= 1 ||
        args.getFlagValues("-h", 0, values) >= 0 ||
        args.getFlagValues("-help", 0, values) >= 0) {
        mHelpRequested = true;
    }

    // required flags -in and -out
    validFlags.push_back("-in");
    if (args.getFlagValues("-in", 1, values) >= 0) {
        mInputSceneFile = values[0];
    } else {
        mMissingRequiredFlags.push_back("-in");
    }

    validFlags.push_back("-out");
    if (args.getFlagValues("-out", 1, values) >= 0) {
        mOutputRdlFile = values[0];
    } else {
        mMissingRequiredFlags.push_back("-out");
    }

    validFlags.push_back("-camera");
    if (args.getFlagValues("-camera", 1, values) >= 0) {
        mCamera = values[0];
    }

    validFlags.push_back("-purpose");
    int foundAtIndex = args.getFlagValues("-purpose", 1, values);
    while (foundAtIndex >= 0) {
        // we are responsible for removing duplicates
        if (std::find(mPurposes.begin(), mPurposes.end(), values[0]) == mPurposes.end()) {
            mPurposes.push_back(values[0]);
        }
        foundAtIndex = args.getFlagValues("-purpose", 1, values, foundAtIndex + 1);
    }

    validFlags.push_back("-refine-level");
    if (args.getFlagValues("-refine-level", 1, values) >= 0) {
        mRefineLevel = std::max(0,std::min(8,atoi(values[0].c_str())));
    }

    validFlags.push_back("-time");
    if (args.getFlagValues("-time",1,values) >= 0) {
        mTimeType = TimeType::DoubleValue;
        mTime = atof(values[0].c_str());
    }

    validFlags.push_back("-set");
    foundAtIndex = args.getFlagValues("-set", -1, values);
    while (foundAtIndex >= 0) {
        // If we have 2 values, assume it is a key/value pair.
        // Anything else interpret as a request to print the list
        // of available settings and exit.
        if (values.size() == 2) {
            mRenderSettings.push_back(RenderSetting(values[0], values[1]));
        } else {
            mPrintRenderSettingsRequested = true;
        }
        foundAtIndex = args.getFlagValues("-set", -1, values, foundAtIndex + 1);
    }

    validFlags.push_back("-size");
    if (args.getFlagValues("-size", 2, values) >= 0) {
        mWidth = atoi(values[0].c_str());
        mHeight = atoi(values[1].c_str());
    }

    mAllFlagsValid = args.allFlagsValid(validFlags);
}

std::string
RenderOptions::usage(const char *argv0) const
{
    using scene_rdl2::util::buildString;

    return buildString("Usage: ", argv0, " [Flags]\n"
                       "-h|-help\n"
                       "    Print this message.\n"
                       "\n"
                       "Required:\n"
                       "-in scene.usd{a}\n"
                       "    Input USD scene data.\n"
                       "\n"
                       "-out scene.rdl[a|b]\n"
                       "    Output RDL file. Extension (.rdla or .rdlb) determines type.\n"
                       "    If you specify a filename with no extension, both .rdla and .rdlb\n"
                       "    will be output, with large array attributes placed in the .rdlb and\n"
                       "    everything else in the .rdla\n"
                       "\n"
                       "Optional:\n"
                       "-camera <CAMERA_NAME>\n"
                       "    Name of the rendering camera.  If not specified, a default\n"
                       "    camera is created that frames the scene geometry.\n"
                       "\n"
                       "-purpose <PURPOSE>\n"
                       "    Specifies a UsdGeomImageable purpose to include in the render.\n"
                       "    <PURPOSE> can be one of 'render', 'proxy', or 'guide'.  This option\n"
                       "    can appear multiple times in order to select multiple purposes.\n"
                       "    The 'default' purpose is implicity set.\n"
                       "\n"
                       "-refine-level <N>\n"
                       "    Set geometry refine level fallback to N. Tessellation rate is\n"
                       "    2^N. 0 disables subdivision, 1 is the default.\n"
                       "\n"
                       "-set <SETTING> <VALUE>.\n"
                       "    Sets the render setting <SETTING> to <VALUE>.  This\n"
                       "    option can appear multiple times.\n"
                       "    Use '-set' to see a list of available settings.\n"
                       "\n"
                       "-size 1920 1080\n"
                       "    Canonical frame width and height (in pixels).\n"
                       "\n"
                       "-time <FRAME>\n"
                       "    Set timecode (i.e. frame) to render at. If not set, uses the special\n"
                       "    value 'Earliest' (see pxr::UsdTimeCode)\n"
                       "\n"
                       );
}

} // namespace hd_render

