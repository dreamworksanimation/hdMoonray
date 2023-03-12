// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RenderOptions.cc

#include "RenderOptions.h"

#include <scene_rdl2/render/util/Args.h>
#include <scene_rdl2/render/util/Strings.h>

#include <stdlib.h>
#include <string>
#include <vector>

namespace hd_render {

// default values
const bool RenderOptions::sHelpRequested = false;

const char * const RenderOptions::sRenderer = "Moonray";

const char * const RenderOptions::sInputSceneFile = "scene.usd";
const char * const RenderOptions::sOutputExrFile = "scene.exr";

const char * const RenderOptions::sCamera = "";

const char * const RenderOptions::sSamplingCamera = "";

const unsigned int RenderOptions::sWidth = 1920;
const unsigned int RenderOptions::sHeight = 1080;
const float RenderOptions::sRes = 1.0f;
const unsigned int RenderOptions::sRefineLevel = 1; // "Complexity/Medium"
const TimeType RenderOptions::sTimeType = TimeType::Earliest;

const char * const RenderOptions::sAov = "color";

const bool RenderOptions::sPrintRenderSettingsRequested = false;

const char * const RenderOptions::sDeltaInputSceneFile = "";
const char * const RenderOptions::sDeltaOutputExrFile = "";

const bool RenderOptions::sUseChromeTraceFormat = false;

const bool RenderOptions::sEnableTiming = false;

RenderOptions::RenderOptions(int argc, char *argv[]):
    // initialize defaults
    mHelpRequested(sHelpRequested),
    mRenderer(sRenderer),
    mInputSceneFile(sInputSceneFile),
    mOutputExrFile(sOutputExrFile),
    mCamera(sCamera),
    mSamplingCamera(sSamplingCamera),
    mWidth(sWidth),
    mHeight(sHeight),
    mRes(sRes),
    mRefineLevel(sRefineLevel),
    mTimeType(sTimeType),
    mAov(sAov),
    mPrintRenderSettingsRequested(sPrintRenderSettingsRequested),
    mDeltaInputSceneFile(sDeltaInputSceneFile),
    mDeltaOutputExrFile(sDeltaOutputExrFile),
    mUseChromeTraceFormat(sUseChromeTraceFormat),
    mEnableTiming(sEnableTiming),
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
        mOutputExrFile = values[0];
    } else {
        mMissingRequiredFlags.push_back("-out");
    }

    // optional flags
    validFlags.push_back("-aov");
    if (args.getFlagValues("-aov", 1, values) >= 0) {
        mAov = values[0];
    }

    validFlags.push_back("-camera");
    if (args.getFlagValues("-camera", 1, values) >= 0) {
        mCamera = values[0];
    }

    validFlags.push_back("-sampling_camera");
    if (args.getFlagValues("-sampling_camera", 1, values) >= 0) {
        mSamplingCamera = values[0];
    }

    validFlags.push_back("-delta_in");
    if (args.getFlagValues("-delta_in", 1, values) >= 0) {
        mDeltaInputSceneFile = values[0];
    }

    validFlags.push_back("-delta_out");
    if (args.getFlagValues("-delta_out", 1, values) >= 0) {
        mDeltaOutputExrFile = values[0];
    }

    validFlags.push_back("-delta_set");
    int foundAtIndex = args.getFlagValues("-delta_set", 2, values);
    while (foundAtIndex >= 0) {
        mDeltaRenderSettings.push_back(RenderSetting(values[0], values[1]));
        foundAtIndex = args.getFlagValues("-delta_set", 2, values, foundAtIndex + 1);
    }

    validFlags.push_back("-renderer");
    if (args.getFlagValues("-renderer", -1, values) >= 0) {
        if (values.size() == 1) {
            mRenderer = values[0];
        } else {
            mRenderer = "";  // will cause a list of renderers to be printed
        }
    }

    validFlags.push_back("-purpose");
    foundAtIndex = args.getFlagValues("-purpose", 1, values);
    while (foundAtIndex >= 0) {
        // we are responsible for removing duplicates
        if (std::find(mPurposes.begin(), mPurposes.end(), values[0]) == mPurposes.end()) {
            mPurposes.push_back(values[0]);
        }
        foundAtIndex = args.getFlagValues("-purpose", 1, values, foundAtIndex + 1);
    }

    validFlags.push_back("-res");
    if (args.getFlagValues("-res", 1, values) >= 0) {
        mRes = atof(values[0].c_str());
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

    validFlags.push_back("-trace");
    foundAtIndex = args.getFlagValues("-trace", -1, values);
    while (foundAtIndex >= 0) {
        if (values.size() == 1) {
            mTraceFiles[values[0]] = "/tmp/trace_"+values[0];
        } else if (values.size() == 2) {
            mTraceFiles[values[0]] = values[1];
        }
        foundAtIndex = args.getFlagValues("-trace", -1, values,foundAtIndex+1);
    }

    validFlags.push_back("-chrome_trace_format");
    if (args.getFlagValues("-chrome_trace_format", 0, values) >= 0) {
        mUseChromeTraceFormat = true;
    }

    validFlags.push_back("-timing");
    if (args.getFlagValues("-timing",-1,values)) {
        mEnableTiming = true;
        if (values.size() > 0) {
            mTimingFile = values[0];
        }
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
                       "-out scene.exr\n"
                       "    Output image name and type.\n"
                       "\n"
                       "Optional:\n"
                       "-aov color\n"
                       "    Name or aov (color, depth, normal, etc...)\n"
                       "\n"
                       "-camera <CAMERA_NAME>\n"
                       "    Name of the rendering camera.  If not specified, a default\n"
                       "    camera is created that frames the scene geometry.\n"
                       "\n"
                       "-sampling_camera <CAMERA_NAME>\n"
                       "     Use this camera's shutter:open and shutter:close to\n"
                       "     define the motion blur sample steps\n"
                       "\n"
                       "-renderer Moonray\n"
                       "    Which hydra render delegate plugin to use.\n"
                       "    Use '-renderer' to see a list of available renderers.\n"
                       "\n"
                       "-delta_in <DELTA>.usd\n"
                       "    After finishing the main render, apply the contents of\n"
                       "    <DELTA>.usd to the scene and re-render the result into\n"
                       "    the file specified with the -delta_out option.\n"
                       "\n"
                       "-delta_out <DELTA>.exr\n"
                       "    Specifies the output delta file.\n"
                       "\n"
                       "-delta_set <SETTING> <VALUE>\n"
                       "    After finishing the main render, apply this render setting\n"
                       "    and re-render the result into the file specified with the -delta_out\n"
                       "    option.  This option can appear multiple times.\n"
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
                       "-res 1.0\n"
                       "    Resolution divisor for frame dimensions.\n"
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
                       "-timing [<file>]\n"
                       "    Write elapsed time in seconds for each step to the given file in simple\n"
                       "    text format. If no file is specified, write timing to stdout\n"
                       "\n"
                       "-trace <STEPNAME> [<FILE>]\n"
                       "    Use the Pixar trace library to trace function calls for the given\n"
                       "    step. Supported step names are: load_plugin, open_stage, populate, render,\n"
                       "    open_delta_stage and delta_render. Writes log to the specified file, or to\n"
                       "    /tmp/trace_<STEPNAME> if no file is specified. Can appear multiple times\n"
                       );
}

} // namespace hd_render

