// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RenderOptions.h

#pragma once

#include <string>
#include <utility>
#include <vector>
#include <map>

namespace hd_usd2rdl {

enum class TimeType
{
    // ways that UsdTimeCode can specify time
    Earliest, Default, DoubleValue
};

// RenderOptions: command line parsing, default values
class RenderOptions
{
public:
    typedef std::pair<std::string, std::string> RenderSetting;

    // Construct render options from command line.  Check for user
    // input errors with the allFlagsValid method.
    RenderOptions(int argc, char *argv[]);

    // Was help requested?
    bool helpRequested() const { return mHelpRequested; }

    // Were all input flags valid?
    bool allFlagsValid() const { return mAllFlagsValid; }

    // Were we mising required flags?
    const std::vector<std::string> &getMissingRequiredFlags() const { return mMissingRequiredFlags; }

    // Return a formatted usage message suitable for printing on
    // stderr or stdout
    std::string usage(const char *argv0) const;

    const std::string &getInputSceneFile() const { return mInputSceneFile; }
    const std::string getOutputRdlFile() const { return mOutputRdlFile; }

    const std::string getCamera() const { return mCamera; }

    unsigned int getWidth() const { return mWidth; }
    unsigned int getHeight() const { return mHeight; }

    unsigned int getRefineLevel() const { return mRefineLevel; }

    TimeType getTimeType() const { return mTimeType; }
    double getTime() const { return mTime; }

    bool getPrintRenderSettingsRequested() const { return mPrintRenderSettingsRequested; }
    const std::vector<RenderSetting> &getRenderSettings() const { return mRenderSettings; }

    // This is the hydra "purpose" it can be one or more of render, proxy, and guide
    const std::vector<std::string> &getPurposes() const { return mPurposes; }


private:
    // default options
    static const bool sHelpRequested;

    static const char * const sInputSceneFile;
    static const char * const sOutputRdlFile;

    static const char * const sCamera;


    static const unsigned int sWidth;
    static const unsigned int sHeight;
    static const unsigned int sRefineLevel;

    static const TimeType sTimeType;


    static const bool sPrintRenderSettingsRequested;

    // parsed results
    bool mHelpRequested;

    std::string mInputSceneFile;
    std::string mOutputRdlFile;

    std::string mCamera;


    unsigned int mWidth;
    unsigned int mHeight;
    unsigned int mRefineLevel;

    TimeType mTimeType;
    double mTime; // interesting if mTimeType = DoubleValue

    std::vector<RenderSetting> mRenderSettings;
    bool mPrintRenderSettingsRequested;

    std::vector<std::string> mPurposes;

   
    // Indicates a user input error
    bool mAllFlagsValid;
    std::vector<std::string> mMissingRequiredFlags;
};

} // namespace hd_render

